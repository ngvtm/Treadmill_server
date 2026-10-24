#include "treadmill.h"
#include "motor/motor.h"
#include "ir_safety/ir_safety.h"
#include "safety_key/safety_key.h"
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
    // Phase 1, item 1.2: every stop (OFF, DANGER, NO_PERSON, hard
    // cutoff) returns the VL53 array to the calm bench budget. This
    // function is the single funnel for all stops, so no stop path
    // can forget the switch.
    ir_safety_set_running_mode(false);
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

/* Reed safety key pulled (fall): cut the motor immediately through
 * the hard-cutoff path, not the gentle controlled stop. The trip is
 * latched in the safety_key module until acknowledged via ON. */
static void on_safety_key_trip(void)
{
    treadmill_hard_cutoff();
}

void treadmill_init(void)
{
    treadmill_reset_stopped_state();
    motor_init();
    safety_key_set_trip_callback(on_safety_key_trip);
    ESP_LOGI(TAG, "Initialized: state=OFF requested_speed=%d", s_requested_speed);
}

void treadmill_handle_command(uint8_t cmd)
{
    switch (cmd) {
    case TREADMILL_CMD_ON: {
        /* Breakaway safety key gate (checked before the VL53 gate):
         * - tripped before: this deliberate ON acts as the
         *   acknowledge, but only clears if the puck is docked again;
         * - puck not docked at all: refuse to start. */
        if (safety_key_is_tripped() && !safety_key_acknowledge()) {
            ESP_LOGE(TAG, "SAFETY BLOCK: reject ON, safety key tripped and not re-docked");
            return;
        }
        if (!safety_key_is_present()) {
            ESP_LOGE(TAG, "SAFETY BLOCK: reject ON, safety key not docked");
            return;
        }

        // Phase 1, item 1.1: a latched sensor fault refuses ON. One
        // deliberate ON with healthy sensors acknowledges the latch
        // (and proceeds, same pattern as the reed safety-key latch);
        // while any safety sensor is still blind the latch stays.
        if (ir_safety_is_fault_latched()) {
            if (!ir_safety_acknowledge_fault()) {
                ESP_LOGE(TAG, "SAFETY BLOCK: reject ON, sensor fault latched and array still blind");
                ir_safety_log_levels();
                return;
            }
            ESP_LOGI(TAG, "SENSOR FAULT acknowledged by ON; proceeding with ON checks");
        }

        if (!ir_safety_is_ready()) {
            ESP_LOGE(TAG, "SAFETY BLOCK: reject ON, VL53 safety not ready");
            ir_safety_log_levels();
            return;
        }

        ir_zone_t zone = ir_safety_get_zone();
        if (zone != IR_ZONE_SAFE) {
            ESP_LOGW(TAG, "SAFETY BLOCK: reject ON, zone=%s", zone_to_str(zone));
            ir_safety_log_levels();
            return;
        }

        if (s_requested_speed == 0) {
            s_requested_speed = TREADMILL_SPEED_DEFAULT;
        }

        s_speed = s_requested_speed;
        s_running = true;
        s_safety_limited = false;

        // Phase 1, item 1.2: the belt is about to move — switch the
        // VL53 array to the short running budget so snapshots stay
        // fresh enough for the 300 ms danger-to-stop target.
        ir_safety_set_running_mode(true);

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

bool treadmill_is_safety_key_tripped(void)
{
    return safety_key_is_tripped();
}

bool treadmill_is_safety_key_present(void)
{
    return safety_key_is_present();
}
