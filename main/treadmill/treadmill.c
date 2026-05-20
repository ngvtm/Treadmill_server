#include "treadmill.h"
#include "motor/motor.h"
#include "ir_safety/ir_safety.h"
#include "esp_log.h"

#define TAG "TREADMILL"
#define TREADMILL_RAMP_NORMAL_MS  300

static bool s_running = false;
static uint8_t s_speed = 0;
static uint8_t s_requested_speed = TREADMILL_SPEED_DEFAULT;
static bool s_safety_limited = false;

static void treadmill_reset_stopped_state(void)
{
    s_running = false;
    s_speed = 0;
    s_requested_speed = TREADMILL_SPEED_DEFAULT;
    s_safety_limited = false;
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

void treadmill_init(void)
{
    treadmill_reset_stopped_state();
    motor_init();
    ESP_LOGI(TAG, "Initialized: state=OFF requested_speed=%d", s_requested_speed);
}

void treadmill_handle_command(uint8_t cmd)
{
    switch (cmd) {
    case TREADMILL_CMD_ON: {
        ir_zone_t zone = ir_safety_get_zone();
        if (zone != IR_ZONE_SAFE) {
            ESP_LOGW(TAG, "SAFETY BLOCK: reject ON, zone=%s", zone_to_str(zone));
            return;
        }

        if (s_requested_speed == 0) {
            s_requested_speed = TREADMILL_SPEED_DEFAULT;
        }

        s_speed = s_requested_speed;
        s_running = true;
        s_safety_limited = false;

        motor_set_direction(MOTOR_DIR_FORWARD);
        motor_ramp_to_speed(s_speed, TREADMILL_RAMP_NORMAL_MS);

        ESP_LOGI(TAG, "ON: speed=%d/%d", s_speed, TREADMILL_SPEED_MAX);
        break;
    }

    case TREADMILL_CMD_OFF:
        motor_controlled_stop(MOTOR_STOP_NORMAL_STEP_MS);
        treadmill_reset_stopped_state();
        ESP_LOGI(TAG, "OFF: controlled stop complete");
        break;

    case TREADMILL_CMD_SPEED_UP:
        if (!s_running) {
            ESP_LOGW(TAG, "Ignore SPEED_UP: treadmill is OFF");
            return;
        }

        if (s_requested_speed >= TREADMILL_SPEED_MAX) {
            ESP_LOGW(TAG, "Already at max speed");
            return;
        }

        s_requested_speed++;
        if (!s_safety_limited) {
            s_speed = s_requested_speed;
            motor_ramp_to_speed(s_speed, TREADMILL_RAMP_NORMAL_MS);
        }

        ESP_LOGI(TAG, "SPEED_UP: requested=%d current=%d/%d",
                 s_requested_speed, s_speed, TREADMILL_SPEED_MAX);
        break;

    case TREADMILL_CMD_SPEED_DOWN:
        if (!s_running) {
            ESP_LOGW(TAG, "Ignore SPEED_DOWN: treadmill is OFF");
            return;
        }

        if (s_requested_speed <= TREADMILL_SPEED_MIN) {
            ESP_LOGW(TAG, "Already at min speed");
            return;
        }

        s_requested_speed--;
        if (!s_safety_limited) {
            s_speed = s_requested_speed;
            motor_ramp_to_speed(s_speed, TREADMILL_RAMP_NORMAL_MS);
        }

        ESP_LOGI(TAG, "SPEED_DOWN: requested=%d current=%d/%d",
                 s_requested_speed, s_speed, TREADMILL_SPEED_MAX);
        break;

    default:
        ESP_LOGW(TAG, "Unknown command: 0x%02X", cmd);
        break;
    }
}

void treadmill_handle_safety_zone(ir_zone_t zone)
{
    if (!s_running) {
        return;
    }

    switch (zone) {
    case IR_ZONE_SAFE:
        if (s_safety_limited) {
            s_safety_limited = false;
            s_speed = s_requested_speed;
            motor_ramp_to_speed(s_speed, TREADMILL_RAMP_NORMAL_MS);
            ESP_LOGI(TAG, "SAFE: restore speed=%d", s_speed);
        }
        break;

    case IR_ZONE_WARNING_1:
        ESP_LOGW(TAG, "WARNING_1: child near warning zone");
        break;

    case IR_ZONE_WARNING_2:
        if (!s_safety_limited) {
            s_safety_limited = true;
            s_speed = s_requested_speed > 1
                    ? s_requested_speed - 1
                    : TREADMILL_SPEED_MIN;
            motor_ramp_to_speed(s_speed, MOTOR_STOP_NORMAL_STEP_MS);
            ESP_LOGW(TAG, "WARNING_2: reduce speed=%d requested=%d",
                     s_speed, s_requested_speed);
        }
        break;

    case IR_ZONE_DANGER:
        motor_controlled_stop(MOTOR_STOP_FAST_STEP_MS);
        treadmill_reset_stopped_state();
        ESP_LOGE(TAG, "DANGER: controlled fast stop");
        break;

    case IR_ZONE_NONE:
    default:
        motor_controlled_stop(MOTOR_STOP_FAST_STEP_MS);
        treadmill_reset_stopped_state();
        ESP_LOGE(TAG, "NO PERSON: controlled fast stop");
        break;
    }
}

void treadmill_hard_cutoff(void)
{
    motor_hard_cutoff();
    treadmill_reset_stopped_state();
    ESP_LOGE(TAG, "HARD CUTOFF: treadmill stopped immediately");
}

bool treadmill_is_running(void)
{
    return s_running;
}

uint8_t treadmill_get_speed(void)
{
    return s_speed;
}

uint8_t treadmill_get_requested_speed(void)
{
    return s_requested_speed;
}

bool treadmill_is_safety_limited(void)
{
    return s_safety_limited;
}
