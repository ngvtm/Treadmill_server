#include "sdkconfig.h"
#include "motor_feedback.h"
#include "motor_feedback_logic.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/pcnt.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Wiring defaults (menuconfig "Treadmill Motor Feedback"). Encoder A
// on GPIO4, B on GPIO33: both non-strapping, input-capable, with
// internal pull-ups available. Heartbeat on GPIO12: a strapping pin
// (MTDI, flash voltage), safe ONLY because the interlock circuit
// presents no pull-up at boot — and the design doc still mandates a
// 10 kOhm pull-down on it so boot strapping reads low (3.3 V flash).
// INA219 shares I2C0 with the VL53 array (SDA 21 / SCL 22, 100 kHz).
#ifndef CONFIG_TREADMILL_ENCODER_GPIO_A
#define CONFIG_TREADMILL_ENCODER_GPIO_A 4
#endif
#ifndef CONFIG_TREADMILL_ENCODER_GPIO_B
#define CONFIG_TREADMILL_ENCODER_GPIO_B 33
#endif
#ifndef CONFIG_TREADMILL_ENCODER_PULSES_PER_REV
#define CONFIG_TREADMILL_ENCODER_PULSES_PER_REV 20
#endif
#ifndef CONFIG_TREADMILL_BELT_LOOP_MM
#define CONFIG_TREADMILL_BELT_LOOP_MM 2800
#endif
#ifndef CONFIG_TREADMILL_INA219_ADDR
#define CONFIG_TREADMILL_INA219_ADDR 64
#endif
#ifndef CONFIG_TREADMILL_INA219_SHUNT_MOHM
#define CONFIG_TREADMILL_INA219_SHUNT_MOHM 100
#endif
#ifndef CONFIG_TREADMILL_INA219_CURRENT_LSB_UA
#define CONFIG_TREADMILL_INA219_CURRENT_LSB_UA 100
#endif
#ifndef CONFIG_TREADMILL_HEARTBEAT_GPIO
#define CONFIG_TREADMILL_HEARTBEAT_GPIO 12
#endif
#ifndef CONFIG_TREADMILL_HEARTBEAT_PERIOD_MS
#define CONFIG_TREADMILL_HEARTBEAT_PERIOD_MS 100
#endif

// INA219 registers.
#define INA219_REG_CONFIG  0x00
#define INA219_REG_CURRENT 0x04
#define INA219_REG_CAL     0x05
// 32 V range, +/-320 mV shunt range, 12-bit, continuous shunt+bus.
#define INA219_CONFIG_VALUE 0x399F

#define TAG "MOTOR_FB"

static motor_feedback_cfg_t s_cfg;
static motor_feedback_state_t s_state;
static bool s_initialized = false;
static bool s_pcnt_ok = false;
static bool s_ina_ok = false;
static int16_t s_last_pcnt = 0;
static TickType_t s_last_poll_tick = 0;
static volatile motor_fault_kind_t s_fault = MOTOR_FAULT_NONE;
static int32_t s_belt_speed_mm_s = 0;
static int32_t s_current_ma = 0;
static uint32_t s_last_degraded_log_ms = 0;

// Forward declarations (treadmill.c owns the running/speed state).
bool treadmill_is_running(void);
uint8_t treadmill_get_speed(void);

// ---- INA219 register access on the shared I2C0 bus ----
static esp_err_t ina219_write_reg16(uint8_t reg, uint16_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CONFIG_TREADMILL_INA219_ADDR << 1) |
                          I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, (value >> 8) & 0xFF, true);
    i2c_master_write_byte(cmd, value & 0xFF, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd,
                                         pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t ina219_read_reg16(uint8_t reg, int16_t *out)
{
    uint8_t data[2] = { 0, 0 };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CONFIG_TREADMILL_INA219_ADDR << 1) |
                          I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CONFIG_TREADMILL_INA219_ADDR << 1) |
                          I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd,
                                         pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (err == ESP_OK) {
        *out = (int16_t)((data[0] << 8) | data[1]);
    }
    return err;
}

// ---- Heartbeat: 10 Hz square wave for the external ENA interlock ----
// If this task stops toggling (firmware hung, task dead), the external
// retriggerable monostable times out (~250 ms) and pulls ENA low,
// stopping the belt without any software involvement. See
// docs/MOTOR_FEEDBACK_DESIGN.md for the circuit.
static void motor_heartbeat_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << CONFIG_TREADMILL_HEARTBEAT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    int level = 0;
    while (1) {
        esp_task_wdt_reset();
        level = !level;
        gpio_set_level(CONFIG_TREADMILL_HEARTBEAT_GPIO, level);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_TREADMILL_HEARTBEAT_PERIOD_MS));
    }
}

void motor_feedback_init(void)
{
    s_cfg = motor_feedback_default_cfg();
#ifndef CONFIG_TREADMILL_MOTOR_STALL_CONFIRM_MS
#define CONFIG_TREADMILL_MOTOR_STALL_CONFIRM_MS 2000
#endif
#ifndef CONFIG_TREADMILL_MOTOR_OVERCURRENT_MA
#define CONFIG_TREADMILL_MOTOR_OVERCURRENT_MA 2500
#endif
#ifndef CONFIG_TREADMILL_MOTOR_OVERCURRENT_CONFIRM_MS
#define CONFIG_TREADMILL_MOTOR_OVERCURRENT_CONFIRM_MS 500
#endif
#ifndef CONFIG_TREADMILL_MOTOR_NO_MOTION_CONFIRM_MS
#define CONFIG_TREADMILL_MOTOR_NO_MOTION_CONFIRM_MS 2000
#endif
#ifndef CONFIG_TREADMILL_MOTOR_NO_MOTION_CURRENT_MA
#define CONFIG_TREADMILL_MOTOR_NO_MOTION_CURRENT_MA 100
#endif
#ifndef CONFIG_TREADMILL_MOTOR_STALL_CURRENT_MA
#define CONFIG_TREADMILL_MOTOR_STALL_CURRENT_MA 500
#endif
#ifndef CONFIG_TREADMILL_MOTOR_FAULT_COOLDOWN_MS
#define CONFIG_TREADMILL_MOTOR_FAULT_COOLDOWN_MS 10000
#endif
    s_cfg.stall_confirm_ms = CONFIG_TREADMILL_MOTOR_STALL_CONFIRM_MS;
    s_cfg.overcurrent_ma = CONFIG_TREADMILL_MOTOR_OVERCURRENT_MA;
    s_cfg.overcurrent_confirm_ms =
        CONFIG_TREADMILL_MOTOR_OVERCURRENT_CONFIRM_MS;
    s_cfg.no_motion_confirm_ms =
        CONFIG_TREADMILL_MOTOR_NO_MOTION_CONFIRM_MS;
    s_cfg.no_motion_current_ma =
        CONFIG_TREADMILL_MOTOR_NO_MOTION_CURRENT_MA;
    s_cfg.stall_current_ma = CONFIG_TREADMILL_MOTOR_STALL_CURRENT_MA;
    s_cfg.fault_cooldown_ms = CONFIG_TREADMILL_MOTOR_FAULT_COOLDOWN_MS;
    motor_feedback_state_init(&s_state);

    // PCNT: legacy driver API, deliberately — it is unchanged across
    // IDF 4 and IDF 5, unlike the IDF 5 pulse_cnt driver. A channel
    // counts rising edges on A; B's level selects count direction
    // (low = forward, high = reverse), so the signed delta gives
    // both speed and direction. The 1000-cycle glitch filter
    // (~12.5 us at 80 MHz APB) rejects contact bounce and noise.
    // PCNT does not touch pull-ups itself; most encoder outputs are
    // open-collector, so enable them (harmless for push-pull outputs).
    gpio_set_pull_mode(CONFIG_TREADMILL_ENCODER_GPIO_A, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CONFIG_TREADMILL_ENCODER_GPIO_B, GPIO_PULLUP_ONLY);
    pcnt_config_t pcnt_cfg = {
        .pulse_gpio_num = CONFIG_TREADMILL_ENCODER_GPIO_A,
        .ctrl_gpio_num = CONFIG_TREADMILL_ENCODER_GPIO_B,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_UNIT_0,
        .pos_mode = PCNT_COUNT_INC,
        .neg_mode = PCNT_COUNT_DIS,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_REVERSE,
    };
    if (pcnt_unit_config(&pcnt_cfg) == ESP_OK &&
        pcnt_set_filter_value(PCNT_UNIT_0, 1000) == ESP_OK &&
        pcnt_filter_enable(PCNT_UNIT_0) == ESP_OK &&
        pcnt_counter_pause(PCNT_UNIT_0) == ESP_OK &&
        pcnt_counter_clear(PCNT_UNIT_0) == ESP_OK &&
        pcnt_counter_resume(PCNT_UNIT_0) == ESP_OK) {
        s_pcnt_ok = true;
    } else {
        ESP_LOGE(TAG, "PCNT init failed: stall/no-motion checks suspended");
    }

    // INA219: the I2C driver is already installed by ir_safety_init
    // (same bus); if this init ever runs first, install it here and
    // tolerate the already-installed error.
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    esp_err_t err = i2c_param_config(I2C_NUM_0, &i2c_cfg);
    if (err == ESP_OK) {
        err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "INA219 bus init failed: %s", esp_err_to_name(err));
    } else {
        uint16_t cal = ina219_calibration(
            CONFIG_TREADMILL_INA219_SHUNT_MOHM,
            CONFIG_TREADMILL_INA219_CURRENT_LSB_UA);
        if (ina219_write_reg16(INA219_REG_CONFIG, INA219_CONFIG_VALUE) ==
                ESP_OK &&
            ina219_write_reg16(INA219_REG_CAL, cal) == ESP_OK) {
            s_ina_ok = true;
            ESP_LOGI(TAG, "INA219 ready: addr=0x%02X cal=%u shunt=%u mOhm lsb=%u uA",
                     CONFIG_TREADMILL_INA219_ADDR, cal,
                     CONFIG_TREADMILL_INA219_SHUNT_MOHM,
                     CONFIG_TREADMILL_INA219_CURRENT_LSB_UA);
        } else {
            ESP_LOGE(TAG, "INA219 not responding: current checks degraded");
        }
    }

    s_last_poll_tick = xTaskGetTickCount();
    s_initialized = true;
    xTaskCreate(motor_heartbeat_task, "motor_hb", 2048, NULL, 10, NULL);
    ESP_LOGI(TAG, "motor feedback ready: pcnt=%d ina=%d heartbeat=GPIO%d",
             s_pcnt_ok, s_ina_ok, CONFIG_TREADMILL_HEARTBEAT_GPIO);
}

motor_fault_kind_t motor_feedback_poll(void)
{
    if (!s_initialized) {
        return MOTOR_FAULT_NONE;
    }
    TickType_t now_tick = xTaskGetTickCount();
    uint32_t now_ms = pdTICKS_TO_MS(now_tick);
    uint32_t window_ms = pdTICKS_TO_MS(now_tick - s_last_poll_tick);
    s_last_poll_tick = now_tick;

    int32_t delta;
    if (s_pcnt_ok) {
        int16_t count = 0;
        if (pcnt_get_counter_value(PCNT_UNIT_0, &count) != ESP_OK) {
            delta = 1;  // unreadable counter: do not trip on silence
        } else {
            // int16 wrap-safe delta (valid while the true delta between
            // polls stays below 32768 counts — at ~160 counts/s max and
            // 100 ms polls, the real delta is ~16).
            delta = (int32_t)(int16_t)(count - s_last_pcnt);
            s_last_pcnt = count;
        }
    } else {
        delta = 1;  // no encoder: stall/no-motion checks suspended
    }

    int32_t current_ma;
    if (s_ina_ok) {
        int16_t raw = 0;
        if (ina219_read_reg16(INA219_REG_CURRENT, &raw) == ESP_OK) {
            current_ma = (int32_t)raw *
                (int32_t)CONFIG_TREADMILL_INA219_CURRENT_LSB_UA / 1000;
        } else {
            // A failed read is not zero current: fail closed toward
            // stall (a silent belt with unknown current stops safely).
            current_ma = (int32_t)s_cfg.stall_current_ma;
        }
    } else {
        current_ma = (int32_t)s_cfg.stall_current_ma;
    }
    s_current_ma = current_ma;
    s_belt_speed_mm_s = motor_feedback_belt_speed_mm_s(
        delta, window_ms, CONFIG_TREADMILL_ENCODER_PULSES_PER_REV,
        CONFIG_TREADMILL_BELT_LOOP_MM);

    if ((!s_pcnt_ok || !s_ina_ok) &&
        now_ms - s_last_degraded_log_ms >= 10000) {
        s_last_degraded_log_ms = now_ms;
        ESP_LOGW(TAG, "motor feedback DEGRADED: pcnt=%d ina=%d (fault checks partial)",
                 s_pcnt_ok, s_ina_ok);
    }

    s_fault = motor_feedback_update(
        &s_state, &s_cfg, treadmill_is_running(),
        treadmill_get_speed(), delta, current_ma, now_ms);
    return s_fault;
}

motor_fault_kind_t motor_feedback_get_fault_kind(void)
{
    return s_fault;
}

int32_t motor_feedback_get_belt_speed_mm_s(void)
{
    return s_belt_speed_mm_s;
}

int32_t motor_feedback_get_current_ma(void)
{
    return s_current_ma;
}
