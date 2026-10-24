#include "sdkconfig.h"
#include "ir_safety.h"
#include "ir_safety_logic.h"

#include <inttypes.h>
#include <new>
#include <stdint.h>

#include "VL53L0X.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TAG "VL53_SAFETY"

static constexpr i2c_port_t I2C_PORT_NUM = I2C_NUM_0;
static constexpr gpio_num_t I2C_SDA_GPIO = GPIO_NUM_21;
static constexpr gpio_num_t I2C_SCL_GPIO = GPIO_NUM_22;
static constexpr uint32_t I2C_SPEED_HZ = 100000;
static constexpr uint32_t I2C_TIMEOUT_MS = 100;
// Timing budget: HIGHER = more samples = more sensitive, LESS noise (fewer
// st=2). 200000us (200ms) is the low-noise end — use this to find the true
// minimum mount height, tested at the same budget you will actually run.
static constexpr uint32_t VL53_TIMING_BUDGET_US = 200000;
// Detection threshold for the SIDE-RAIL / CROSS-SCAN geometry:
// sensors sit on a rail running along the belt length and look ACROSS the belt.
// Each sensor is one vertical slice cutting the belt width.
//
// Mount geometry: the PCB seat stands almost VERTICAL - 77.5 deg from the belt
// surface, i.e. leaning back just 12.5 deg from vertical. The optical axis is
// normal to the PCB, so it rises 12.5 deg above horizontal. With the VL53L0X
// 25 deg cone (+/-12.5 deg) that puts the LOWEST ray at 0 deg - exactly parallel
// to the belt surface, skimming just above it at the mount height. That is the
// design intent: full width covered, no ray aimed into the belt itself.
//
// Because the axis is tilted, the reported value is a SLANT range, not the
// horizontal distance:  slant = horizontal / cos(12.5 deg) = horizontal * 1.024
//
// Belt is 800 mm (length) x 650 mm (width):
//   horizontal 200 mm (near the rail)  -> slant 205 mm
//   horizontal 325 mm (belt centre)    -> slant 333 mm
//   horizontal 650 mm (far belt edge)  -> slant 666 mm   <- must still detect
//
// So the threshold must be >= 666 mm to cover the full belt width. 700 mm keeps
// full coverage with a 34 mm margin for mount tolerance.
//
// IMPORTANT - what must be true on the far side for this to work:
// nothing fixed may sit within 700 mm of any sensor, otherwise that structure is
// permanently "detected" and the zone latches (a DANGER-role sensor would keep
// the belt from ever starting). If a baseline target strip is fitted, mount it
// at >= 780 mm horizontal (slant >= 799 mm) so it stays clear of this threshold.
// Verify with an empty belt: every sensor must read > 700 mm or status 4.
// Detection threshold now comes from menuconfig (Treadmill
// Configuration -> TREADMILL_VL53_DETECT_MAX_MM, default 700 mm)
// so bench tuning does not require a code change.
static constexpr uint16_t VL53_DETECT_MAX_MM = CONFIG_TREADMILL_VL53_DETECT_MAX_MM;
static constexpr uint32_t XSHUT_RESET_MS = 800;
static constexpr uint32_t XSHUT_BOOT_MS = 800;
static constexpr uint32_t STANDARD_INIT_ATTEMPTS = 4;
static constexpr uint32_t READ_ATTEMPTS = 3;
static constexpr uint32_t READ_RETRY_DELAY_MS = 30;
// Set to 1 while bench-testing mount height: HEALTH prints EVERY cycle with raw
// distances so you can watch d change as you lower a sensor. Set back to 0 for
// normal use (prints only on zone change) so it does not flood motor/BLE logs.
#define IR_RAW_DISTANCE_LOG 1

static constexpr size_t SENSOR_COUNT = 8;
static constexpr gpio_num_t XSHUT_GPIOS[SENSOR_COUNT] = {
    GPIO_NUM_13,   // S0
    GPIO_NUM_14,   // S1
    GPIO_NUM_32,   // S2
    GPIO_NUM_27,   // S3
    GPIO_NUM_26,   // S4
    GPIO_NUM_25,   // S5
    GPIO_NUM_16,   // S6
    GPIO_NUM_17,   // S7
};
static constexpr uint8_t DEFAULT_ADDR = 0x29;
static constexpr uint8_t SENSOR_ADDRS[SENSOR_COUNT] = {
    0x30,
    0x31,
    0x32,
    0x33,
    0x34,
    0x35,
    0x36,
    0x37,
};
static constexpr size_t INIT_ORDER[SENSOR_COUNT] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    0,
};
static constexpr bool SENSOR_ENABLED[SENSOR_COUNT] = {
    true,   // S0
    true,   // S1
    true,   // S2
    true,   // S3
    true,   // S4
    true,   // S5
    true,   // S6
    true,   // S7
};
// Safety role per sensor index. Keep in sync with SENSOR_ENABLED: an enabled
// sensor must have a real role, a disabled one must be IR_ROLE_UNUSED.
// Symmetric 2-2-2-2 layout, front (safe) -> rear (danger).
static constexpr ir_role_t SENSOR_ROLE[SENSOR_COUNT] = {
    IR_ROLE_SAFE,       // S0
    IR_ROLE_SAFE,       // S1
    IR_ROLE_WARNING_1,  // S2
    IR_ROLE_WARNING_1,  // S3
    IR_ROLE_WARNING_2,  // S4
    IR_ROLE_WARNING_2,  // S5
    IR_ROLE_DANGER,     // S6
    IR_ROLE_DANGER,     // S7
};

static VL53L0X *s_sensors[SENSOR_COUNT] = {};
static bool s_sensor_active[SENSOR_COUNT] = {};
static uint16_t s_distance_mm[SENSOR_COUNT] = {};
static uint8_t s_range_status[SENSOR_COUNT] = {};
static bool s_detected[SENSOR_COUNT] = {};
static bool s_initialized = false;
static bool s_ready = false;
static ir_zone_t s_cached_zone = IR_ZONE_NONE;
static SemaphoreHandle_t s_read_mutex = nullptr;
static uint32_t s_read_error_count[SENSOR_COUNT] = {};
static uint32_t s_reset_count[SENSOR_COUNT] = {};
static uint32_t s_last_reset_ms[SENSOR_COUNT] = {};
static uint32_t s_recover_count[SENSOR_COUNT] = {};

// Phase 1, item 1.1: asymmetric temporal zone filter + sensor-fault latch.
// Tuned via menuconfig (Treadmill Configuration menu); defaults below are
// the Phase 1 plan values. The filter is time-based, not sweep-count-based,
// because sweep duration changes ~30x once parallel ranging (item 1.2) lands.
#ifndef CONFIG_TREADMILL_ZONE_CLEAN_MS
#define CONFIG_TREADMILL_ZONE_CLEAN_MS 500
#endif
#ifndef CONFIG_TREADMILL_ZONE_NONE_CONFIRM_MS
#define CONFIG_TREADMILL_ZONE_NONE_CONFIRM_MS 400
#endif
#ifndef CONFIG_TREADMILL_FAULT_BLIND_LATCH_SNAPSHOTS
#define CONFIG_TREADMILL_FAULT_BLIND_LATCH_SNAPSHOTS 5
#endif
static ir_zone_filter_t s_zone_filter = {};
static ir_fault_latch_t s_fault_latch = {};
static const ir_zone_filter_cfg_t s_zone_filter_cfg = {
    .clean_ms = CONFIG_TREADMILL_ZONE_CLEAN_MS,
    .none_confirm_ms = CONFIG_TREADMILL_ZONE_NONE_CONFIRM_MS,
};

static uint64_t xshut_pin_mask(void)
{
    uint64_t mask = 0;
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        mask |= 1ULL << XSHUT_GPIOS[i];
    }
    return mask;
}

static size_t enabled_sensor_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        if (SENSOR_ENABLED[i]) {
            count++;
        }
    }
    return count;
}

static void set_xshut_level(size_t index, int level)
{
    esp_err_t err = gpio_set_level(XSHUT_GPIOS[index], level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_level(GPIO%d, %d) failed: %s",
                 XSHUT_GPIOS[index], level, esp_err_to_name(err));
    }
}

static void set_all_xshut_level(int level)
{
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        set_xshut_level(i, level);
    }
}

static void log_xshut_levels(const char *phase)
{
    char buf[160];
    int n = 0;
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        n += snprintf(buf + n, sizeof(buf) - n, " S%u/GPIO%d=%d",
                      (unsigned)i, XSHUT_GPIOS[i], gpio_get_level(XSHUT_GPIOS[i]));
        if (n >= (int)sizeof(buf)) {
            break;
        }
    }
    ESP_LOGI(TAG, "%s XSHUT levels:%s", phase, buf);
}

static esp_err_t i2c_probe_err(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT_NUM, cmd,
                                         pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}

static bool i2c_probe(uint8_t addr)
{
    return i2c_probe_err(addr) == ESP_OK;
}

static esp_err_t read_reg8(uint8_t addr, uint8_t reg, uint8_t *value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT_NUM, cmd,
                                         pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}

static bool is_vl53l0x(uint8_t addr)
{
    uint8_t model = 0;
    uint8_t type = 0;
    esp_err_t model_err = read_reg8(addr, 0xC0, &model);
    esp_err_t type_err = read_reg8(addr, 0xC1, &type);
    if (model_err == ESP_OK && type_err == ESP_OK) {
        ESP_LOGI(TAG, "VL53L0X ID at 0x%02X: model_id=0x%02X type=0x%02X",
                 addr, model, type);
    }

    return model_err == ESP_OK && type_err == ESP_OK && model == 0xEE;
}

static void log_runtime_i2c_diagnostic(size_t index, uint8_t status, uint32_t attempts)
{
    s_read_error_count[index]++;
    if (s_read_error_count[index] > 3 && (s_read_error_count[index] % 10) != 0) {
        return;
    }

    uint8_t addr = SENSOR_ADDRS[index];
    uint8_t model = 0;
    uint8_t type = 0;
    esp_err_t target_probe = i2c_probe_err(addr);
    esp_err_t default_probe = i2c_probe_err(DEFAULT_ADDR);
    esp_err_t model_err = read_reg8(addr, 0xC0, &model);
    esp_err_t type_err = read_reg8(addr, 0xC1, &type);

    uint32_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    bool sensor_reset = (target_probe != ESP_OK) && (default_probe == ESP_OK);
    if (sensor_reset) {
        s_reset_count[index]++;
        uint32_t gap_ms = s_last_reset_ms[index] ? now_ms - s_last_reset_ms[index] : 0;
        s_last_reset_ms[index] = now_ms;
        ESP_LOGE(TAG,
                 "S%u RESET DETECTED: dropped 0x%02X -> back on 0x29 at t=%" PRIu32
                 " ms reset_count=%" PRIu32 " since_last=%" PRIu32 " ms (power/wiring glitch)",
                 (unsigned)index, addr, now_ms,
                 s_reset_count[index], gap_ms);
    }

    ESP_LOGW(TAG,
             "S%u read failed st=%u count=%" PRIu32 " attempts=%" PRIu32
             " target=0x%02X target_probe=%s default_0x29=%s"
             " id_model=%s/0x%02X id_type=%s/0x%02X XSHUT_GPIO%d=%d t=%" PRIu32 " ms",
             (unsigned)index,
             status,
             s_read_error_count[index],
             attempts,
             addr,
             esp_err_to_name(target_probe),
             esp_err_to_name(default_probe),
             esp_err_to_name(model_err),
             model,
             esp_err_to_name(type_err),
             type,
             XSHUT_GPIOS[index],
             gpio_get_level(XSHUT_GPIOS[index]),
             now_ms);
}

static void reset_all_sensors_for_address_assignment(void)
{
    ESP_LOGI(TAG, "Reset all VL53L0X sensors before address assignment");
    set_all_xshut_level(0);
    vTaskDelay(pdMS_TO_TICKS(XSHUT_RESET_MS));
    ESP_LOGI(TAG, "Default address 0x%02X present after reset=%d",
             DEFAULT_ADDR, i2c_probe(DEFAULT_ADDR) ? 1 : 0);
}

static const char *zone_to_str(ir_zone_t zone)
{
    switch (zone) {
    case IR_ZONE_NONE:      return "NONE";
    case IR_ZONE_SAFE:      return "SAFE";
    case IR_ZONE_WARNING_1: return "WARNING_1";
    case IR_ZONE_WARNING_2: return "WARNING_2";
    case IR_ZONE_DANGER:    return "DANGER";
    default:                return "UNKNOWN";
    }
}

static void configure_xshut_pins(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = xshut_pin_mask(),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    set_all_xshut_level(0);
    vTaskDelay(pdMS_TO_TICKS(300));
}

static void power_cycle_sensor(size_t index, const char *reason)
{
    ESP_LOGW(TAG, "Power-cycle S%u XSHUT=GPIO%d: %s",
             (unsigned)index, XSHUT_GPIOS[index], reason);
    set_xshut_level(index, 0);
    vTaskDelay(pdMS_TO_TICKS(XSHUT_RESET_MS));
    set_xshut_level(index, 1);
    vTaskDelay(pdMS_TO_TICKS(XSHUT_BOOT_MS));
    log_xshut_levels("After power-cycle");
}

static bool standard_init_with_retries(VL53L0X *sensor, size_t index)
{
    for (uint32_t attempt = 1; attempt <= STANDARD_INIT_ATTEMPTS; attempt++) {
        TickType_t start_tick = xTaskGetTickCount();
        ESP_LOGI(TAG, "S%u standard init attempt %u/%u begin",
                 (unsigned)index, (unsigned)attempt, (unsigned)STANDARD_INIT_ATTEMPTS);

        bool init_ok = false;
        if (!i2c_probe(DEFAULT_ADDR) || !is_vl53l0x(DEFAULT_ADDR)) {
            ESP_LOGI(TAG, "S%u default address not ready before standard init attempt %u/%u",
                     (unsigned)index, (unsigned)attempt, (unsigned)STANDARD_INIT_ATTEMPTS);
        } else {
            init_ok = sensor->initQuiet(true);
        }

        uint32_t elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - start_tick);
        if (init_ok) {
            if (attempt > 1) {
                ESP_LOGI(TAG, "S%u standard init succeeded on attempt %u/%u after %" PRIu32 " ms",
                         (unsigned)index, (unsigned)attempt,
                         (unsigned)STANDARD_INIT_ATTEMPTS, elapsed_ms);
            } else {
                ESP_LOGI(TAG, "S%u standard init succeeded after %" PRIu32 " ms",
                         (unsigned)index, elapsed_ms);
            }
            return true;
        }

        ESP_LOGW(TAG, "S%u standard init attempt %u/%u failed after %" PRIu32 " ms",
                 (unsigned)index, (unsigned)attempt,
                 (unsigned)STANDARD_INIT_ATTEMPTS, elapsed_ms);

        if (attempt < STANDARD_INIT_ATTEMPTS) {
            ESP_LOGI(TAG, "S%u standard init attempt %u/%u failed; retry after XSHUT reset",
                     (unsigned)index, (unsigned)attempt, (unsigned)STANDARD_INIT_ATTEMPTS);
            set_xshut_level(index, 0);
            vTaskDelay(pdMS_TO_TICKS(XSHUT_RESET_MS));
            set_xshut_level(index, 1);
            vTaskDelay(pdMS_TO_TICKS(XSHUT_BOOT_MS));
        }
    }

    return false;
}

static bool init_one_sensor(size_t index)
{
    if (!SENSOR_ENABLED[index]) {
        ESP_LOGI(TAG, "S%u disabled; skip init", (unsigned)index);
        set_xshut_level(index, 0);
        s_sensor_active[index] = false;
        return false;
    }

    VL53L0X *sensor = s_sensors[index];

    set_xshut_level(index, 1);
    vTaskDelay(pdMS_TO_TICKS(XSHUT_BOOT_MS));
    ESP_LOGI(TAG, "Enable S%u XSHUT=GPIO%d target=0x%02X",
             (unsigned)index, XSHUT_GPIOS[index], SENSOR_ADDRS[index]);
    log_xshut_levels("Before init");

    if (!i2c_probe(DEFAULT_ADDR)) {
        if (i2c_probe(SENSOR_ADDRS[index]) && is_vl53l0x(SENSOR_ADDRS[index])) {
            ESP_LOGI(TAG, "S%u already at target address 0x%02X",
                     (unsigned)index, SENSOR_ADDRS[index]);
            if (!sensor->initAtAddressQuiet(SENSOR_ADDRS[index])) {
                ESP_LOGE(TAG, "S%u initAtAddress failed at 0x%02X",
                         (unsigned)index, SENSOR_ADDRS[index]);
                set_xshut_level(index, 0);
                return false;
            }
            sensor->setTimingBudget(VL53_TIMING_BUDGET_US);
            s_sensor_active[index] = true;
            ESP_LOGI(TAG, "S%u ready at 0x%02X", (unsigned)index, SENSOR_ADDRS[index]);
            return true;
        }

        power_cycle_sensor(index, "default address missing or target address invalid");
        if (!i2c_probe(DEFAULT_ADDR)) {
            ESP_LOGE(TAG, "S%u not found at default address 0x%02X after XSHUT reset",
                     (unsigned)index, DEFAULT_ADDR);
            set_xshut_level(index, 0);
            return false;
        }
    }

    if (!is_vl53l0x(DEFAULT_ADDR)) {
        ESP_LOGE(TAG, "S%u at 0x%02X is not VL53L0X-compatible",
                 (unsigned)index, DEFAULT_ADDR);
        set_xshut_level(index, 0);
        return false;
    }

    if (!standard_init_with_retries(sensor, index)) {
        ESP_LOGE(TAG, "S%u standard init failed after %u attempts at 0x%02X",
                 (unsigned)index, (unsigned)STANDARD_INIT_ATTEMPTS, DEFAULT_ADDR);
        set_xshut_level(index, 0);
        return false;
    }
    sensor->setTimingBudget(VL53_TIMING_BUDGET_US);

    if (!sensor->setDeviceAddress(SENSOR_ADDRS[index])) {
        ESP_LOGE(TAG, "S%u address change failed: 0x%02X -> 0x%02X",
                 (unsigned)index, DEFAULT_ADDR, SENSOR_ADDRS[index]);
        set_xshut_level(index, 0);
        return false;
    }

    if (!is_vl53l0x(SENSOR_ADDRS[index])) {
        ESP_LOGE(TAG, "S%u did not respond at target address 0x%02X",
                 (unsigned)index, SENSOR_ADDRS[index]);
        set_xshut_level(index, 0);
        return false;
    }

    s_sensor_active[index] = true;
    ESP_LOGI(TAG, "S%u ready at 0x%02X", (unsigned)index, SENSOR_ADDRS[index]);
    return true;
}

// Attempt to bring a sensor that reset back to the default address (0x29)
// back onto its assigned address. Returns true if recovered. Cheap probe
// first so the normal (healthy) path costs almost nothing.
// Recover every sensor that dropped back to the default address. A single
// power glitch can knock several sensors off at once; if two or more sit on
// 0x29 together they collide on the bus and any init reads garbage. So isolate
// first: park all lost sensors in shutdown, then bring them up one at a time
// so exactly one chip owns 0x29 during its re-init.
static void recover_lost_sensors(void)
{
    size_t lost[SENSOR_COUNT];
    size_t lost_n = 0;

    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        if (!SENSOR_ENABLED[i] || s_sensors[i] == nullptr) {
            continue;
        }
        if (!i2c_probe(SENSOR_ADDRS[i])) {
            lost[lost_n++] = i;
        }
    }
    if (lost_n == 0) {
        return;
    }

    ESP_LOGW(TAG, "auto-recover: %u sensor(s) lost their address; isolating XSHUT",
             (unsigned)lost_n);

    for (size_t k = 0; k < lost_n; k++) {
        set_xshut_level(lost[k], 0);
    }
    vTaskDelay(pdMS_TO_TICKS(XSHUT_RESET_MS));

    for (size_t k = 0; k < lost_n; k++) {
        size_t i = lost[k];
        set_xshut_level(i, 1);
        vTaskDelay(pdMS_TO_TICKS(XSHUT_BOOT_MS));

        if (!i2c_probe(DEFAULT_ADDR) || !is_vl53l0x(DEFAULT_ADDR)) {
            ESP_LOGW(TAG, "S%u auto-recover: not present at 0x%02X",
                     (unsigned)i, DEFAULT_ADDR);
            s_sensor_active[i] = false;
            continue;
        }
        if (!s_sensors[i]->initQuiet(false)) {
            ESP_LOGW(TAG, "S%u auto-recover: init failed", (unsigned)i);
            s_sensor_active[i] = false;
            continue;
        }
        s_sensors[i]->setTimingBudget(VL53_TIMING_BUDGET_US);
        if (!s_sensors[i]->setDeviceAddress(SENSOR_ADDRS[i]) ||
            !is_vl53l0x(SENSOR_ADDRS[i])) {
            ESP_LOGW(TAG, "S%u auto-recover: address change failed",
                     (unsigned)i);
            s_sensor_active[i] = false;
            continue;
        }

        s_sensor_active[i] = true;
        s_recover_count[i]++;
        ESP_LOGI(TAG, "S%u auto-recover OK at 0x%02X recover_count=%" PRIu32,
                 (unsigned)i, SENSOR_ADDRS[i], s_recover_count[i]);
    }
}

static bool refresh_sensor(size_t index)
{
    s_detected[index] = false;
    s_distance_mm[index] = 0;
    s_range_status[index] = 255;

    if (!s_sensor_active[index] || s_sensors[index] == nullptr) {
        return false;
    }

    uint16_t distance = 0;
    uint8_t status = 255;
    bool ok = false;
    uint32_t attempts_used = 0;
    for (uint32_t attempt = 1; attempt <= READ_ATTEMPTS; attempt++) {
        attempts_used = attempt;
        ok = s_sensors[index]->readWithStatus(&distance, &status);
        if (ok || (status != 253 && status != 254)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(READ_RETRY_DELAY_MS));
    }
    s_distance_mm[index] = distance;
    s_range_status[index] = status;
    // Detection gated on a VALID status (not just an in-range distance), via
    // the same pure predicate the unit tests exercise.
    ir_sensor_reading_t r = {
        .role = SENSOR_ROLE[index],
        .enabled = SENSOR_ENABLED[index],
        .active = s_sensor_active[index],
        .status = status,
        .distance_mm = distance,
    };
    s_detected[index] = ir_logic_is_detected(&r, VL53_DETECT_MAX_MM);
    if (ok) {
        s_read_error_count[index] = 0;
    } else if (status == 253 || status == 254) {
        log_runtime_i2c_diagnostic(index, status, attempts_used);
    }
    return ok;
}

// Inter-sensor delay in the scan loop. Lower = faster full-array scan.
static constexpr uint32_t SCAN_INTER_SENSOR_MS = 5;

static void refresh_all_sensors(void)
{
    bool any_failed = false;
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        if (!refresh_sensor(i) && SENSOR_ENABLED[i] && s_sensor_active[i]) {
            any_failed = true;
        }
        vTaskDelay(pdMS_TO_TICKS(SCAN_INTER_SENSOR_MS));
    }
    // Recover once per sweep, not per sensor: a glitch usually takes several
    // sensors down together and they must be re-addressed as a group.
    if (any_failed) {
        recover_lost_sensors();
    }
}

static void build_snapshot(ir_sensor_reading_t out[SENSOR_COUNT])
{
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        out[i].role = SENSOR_ROLE[i];
        out[i].enabled = SENSOR_ENABLED[i];
        out[i].active = s_sensor_active[i];
        out[i].status = s_range_status[i];
        out[i].distance_mm = s_distance_mm[i];
    }
}

// Raw effective zone -> fault-latch update -> temporal filter.
// Every caller (get_zone, health_check) goes through this one path, so
// the filtered zone is the only zone the rest of the firmware ever sees.
static ir_zone_t calculate_zone_from_snapshot(void)
{
    ir_sensor_reading_t snap[SENSOR_COUNT];
    build_snapshot(snap);
    ir_zone_t raw = ir_logic_effective_zone(snap, SENSOR_COUNT, VL53_DETECT_MAX_MM);
    bool blind = ir_logic_any_safety_sensor_blind(snap, SENSOR_COUNT);
    bool was_latched = s_fault_latch.latched;
    ir_fault_latch_update(&s_fault_latch, blind,
                          CONFIG_TREADMILL_FAULT_BLIND_LATCH_SNAPSHOTS);
    if (!was_latched && s_fault_latch.latched) {
        ESP_LOGE(TAG, "SENSOR FAULT LATCHED: safety sensor blind %u consecutive snapshots; "
                      "ON refused until a deliberate ON with healthy sensors acknowledges it",
                 (unsigned)s_fault_latch.blind_streak);
    }
    uint32_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    return ir_zone_filter_update(&s_zone_filter, raw, now_ms, &s_zone_filter_cfg);
}

bool ir_safety_is_fault_latched(void)
{
    return s_fault_latch.latched;
}

// Acknowledge a latched sensor fault. Refreshes the array first so a
// sensor that recovered since the last snapshot is seen as healthy and
// one deliberate ON both acknowledges and proceeds (same pattern as the
// reed safety-key latch). Returns false while any safety sensor is still
// blind — the latch (and the ON refusal) then stays in place.
bool ir_safety_acknowledge_fault(void)
{
    if (!s_fault_latch.latched) {
        return true;
    }
    if (s_read_mutex != nullptr) {
        xSemaphoreTake(s_read_mutex, portMAX_DELAY);
    }
    refresh_all_sensors();
    ir_sensor_reading_t snap[SENSOR_COUNT];
    build_snapshot(snap);
    bool blind = ir_logic_any_safety_sensor_blind(snap, SENSOR_COUNT);
    if (s_read_mutex != nullptr) {
        xSemaphoreGive(s_read_mutex);
    }
    bool ok = ir_fault_latch_acknowledge(&s_fault_latch, blind);
    if (ok) {
        ESP_LOGI(TAG, "SENSOR FAULT acknowledged: array healthy again");
    }
    return ok;
}

void ir_safety_init(void)
{
    configure_xshut_pins();
    s_read_mutex = xSemaphoreCreateMutex();
    if (s_read_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create VL53 read mutex");
    }

    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        s_sensors[i] = new (std::nothrow) VL53L0X(I2C_PORT_NUM, GPIO_NUM_MAX, GPIO_NUM_MAX);
        if (s_sensors[i] == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate VL53L0X object S%u", (unsigned)i);
            continue;
        }
    }

    if (s_sensors[0] != nullptr) {
        s_sensors[0]->i2cMasterInit(I2C_SDA_GPIO, I2C_SCL_GPIO, I2C_SPEED_HZ);
    }

    ESP_LOGI(TAG, "Initialized bus: SDA=GPIO%d SCL=GPIO%d XSHUT={13,14,32,27,26,25,16,17}",
             I2C_SDA_GPIO, I2C_SCL_GPIO);

    reset_all_sensors_for_address_assignment();

    size_t active_count = 0;
    size_t required_count = enabled_sensor_count();
    for (size_t order_index = 0; order_index < SENSOR_COUNT; order_index++) {
        size_t i = INIT_ORDER[order_index];
        if (!SENSOR_ENABLED[i]) {
            ESP_LOGI(TAG, "S%u disabled; keep XSHUT low", (unsigned)i);
            set_xshut_level(i, 0);
            continue;
        }
        if (init_one_sensor(i)) {
            active_count++;
        } else {
            ESP_LOGE(TAG, "S%u unavailable; check XSHUT GPIO%d and wiring",
                     (unsigned)i, XSHUT_GPIOS[i]);
        }
    }

    s_initialized = active_count > 0;
    s_ready = active_count == required_count;
    if (s_ready) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ir_safety_get_zone();
        ir_safety_log_levels();
    }
    ESP_LOGI(TAG, "VL53 safety initialized: active=%u/%u detect<=%u mm enabled={S0..S7} zones: SAFE={S0,S1} WARNING_1={S2,S3} WARNING_2={S4,S5} DANGER={S6,S7} (standard init only; blind sensor => DANGER; auto-recover on 0x29)",
             (unsigned)active_count, (unsigned)required_count, VL53_DETECT_MAX_MM);
    if (!s_ready) {
        ESP_LOGE(TAG, "VL53 safety is NOT READY: require all %u enabled sensors active before treadmill ON",
                 (unsigned)required_count);
    }
}

ir_zone_t ir_safety_get_zone(void)
{
    if (!s_initialized) {
        return IR_ZONE_NONE;
    }

    if (s_read_mutex != nullptr) {
        xSemaphoreTake(s_read_mutex, portMAX_DELAY);
    }

    refresh_all_sensors();
    s_cached_zone = calculate_zone_from_snapshot();

    if (s_read_mutex != nullptr) {
        xSemaphoreGive(s_read_mutex);
    }

    return s_cached_zone;
}

ir_zone_t ir_safety_get_cached_zone(void)
{
    return s_cached_zone;
}

bool ir_safety_is_ready(void)
{
    // Re-evaluate against the latest snapshot, not just the boot-time flag:
    // a sensor that went blind or dropped offline at runtime makes us unready.
    if (!s_ready) {
        return false;
    }
    ir_sensor_reading_t snap[SENSOR_COUNT];
    build_snapshot(snap);
    return ir_logic_is_ready(snap, SENSOR_COUNT);
}

bool ir_safety_is_safe(void)
{
    if (!s_ready) {
        return false;
    }
    if (s_read_mutex != nullptr) {
        xSemaphoreTake(s_read_mutex, portMAX_DELAY);
    }
    refresh_all_sensors();
    ir_sensor_reading_t snap[SENSOR_COUNT];
    build_snapshot(snap);
    s_cached_zone = ir_logic_effective_zone(snap, SENSOR_COUNT, VL53_DETECT_MAX_MM);
    bool safe = ir_logic_is_safe_to_run(snap, SENSOR_COUNT, VL53_DETECT_MAX_MM);
    if (s_read_mutex != nullptr) {
        xSemaphoreGive(s_read_mutex);
    }
    return safe;
}

void ir_safety_log_levels(void)
{
    char buf[320];
    int n = 0;
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        n += snprintf(buf + n, sizeof(buf) - n,
                      " S%u{act=%d det=%d dist=%u st=%u}",
                      (unsigned)i, s_sensor_active[i], s_detected[i],
                      s_distance_mm[i], s_range_status[i]);
        if (n >= (int)sizeof(buf)) {
            break;
        }
    }
    ESP_LOGI(TAG, "zone=%s threshold=%u%s",
             zone_to_str(s_cached_zone), VL53_DETECT_MAX_MM, buf);
}

void ir_safety_health_check(void)
{
    if (!s_initialized) {
        return;
    }

    if (s_read_mutex != nullptr) {
        xSemaphoreTake(s_read_mutex, portMAX_DELAY);
    }

    refresh_all_sensors();
    s_cached_zone = calculate_zone_from_snapshot();

    // Keep refreshing (so zone stays current and auto-recover still runs), but
    // only emit a line when the zone actually changes, so it does not drown out
    // motor/BLE logs during full-flow testing.
    static ir_zone_t s_last_logged_zone = IR_ZONE_NONE;
    static bool s_health_logged_once = false;
    if (IR_RAW_DISTANCE_LOG || !s_health_logged_once || s_cached_zone != s_last_logged_zone) {
        s_health_logged_once = true;
        s_last_logged_zone = s_cached_zone;
        uint32_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        char buf[320];
        int m = 0;
        for (size_t i = 0; i < SENSOR_COUNT; i++) {
            m += snprintf(buf + m, sizeof(buf) - m, " S%u{a=%d st=%u d=%u}",
                          (unsigned)i, s_sensor_active[i],
                          s_range_status[i], s_distance_mm[i]);
            if (m >= (int)sizeof(buf)) {
                break;
            }
        }
        ESP_LOGI(TAG, "HEALTH zone->%s t=%" PRIu32 " ms%s",
                 zone_to_str(s_cached_zone), now_ms, buf);
    }

    if (s_read_mutex != nullptr) {
        xSemaphoreGive(s_read_mutex);
    }
}
