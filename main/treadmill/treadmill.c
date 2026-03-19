#include "treadmill.h"
#include "motor/motor.h"
#include "esp_log.h"

#define TAG "TREADMILL"

static bool    s_running = false;
static uint8_t s_speed   = TREADMILL_SPEED_MIN;

void treadmill_init(void)
{
    s_running = false;
    s_speed   = TREADMILL_SPEED_MIN;
    motor_init();
    ESP_LOGI(TAG, "Initialized (state=OFF, speed=%d)", s_speed);
}

void treadmill_handle_command(uint8_t cmd)
{
    switch (cmd) {

    case TREADMILL_CMD_ON:
        s_running = true;
        motor_set_direction(MOTOR_DIR_FORWARD);
        motor_set_speed(s_speed);
        ESP_LOGI(TAG, ">>> BẬT MÁY CHẠY | Speed hiện tại: %d/%d",
                 s_speed, TREADMILL_SPEED_MAX);
        /* TODO: gpio_set_level(MOTOR_EN_GPIO, 1); */
        /* TODO: pwm_set_speed(s_speed);           */
        break;

    case TREADMILL_CMD_OFF:
        s_running = false;
        motor_stop();
        ESP_LOGI(TAG, ">>> TẮT MÁY CHẠY");
        /* TODO: gpio_set_level(MOTOR_EN_GPIO, 0); */
        /* TODO: pwm_set_duty(0);                  */
        break;

    case TREADMILL_CMD_SPEED_UP:
        if (s_speed < TREADMILL_SPEED_MAX) {
            s_speed++;
            ESP_LOGI(TAG, ">>> TĂNG VẬN TỐC -> Speed: %d/%d",
                     s_speed, TREADMILL_SPEED_MAX);
            if (s_running) motor_set_speed(s_speed);
            /* TODO: if (s_running) pwm_set_speed(s_speed); */
        } else {
            ESP_LOGW(TAG, "Đã đạt tốc độ tối đa (%d/%d)",
                     s_speed, TREADMILL_SPEED_MAX);
        }
        break;

    case TREADMILL_CMD_SPEED_DOWN:
        if (s_speed > TREADMILL_SPEED_MIN) {
            s_speed--;
            ESP_LOGI(TAG, ">>> GIẢM VẬN TỐC -> Speed: %d/%d",
                     s_speed, TREADMILL_SPEED_MAX);
            if (s_running) {
                if (s_speed == 0) {
                    motor_stop();
                } else {
                    motor_set_speed(s_speed);
                }
            }
            /* TODO: if (s_running) pwm_set_speed(s_speed); */
        } else {
            ESP_LOGW(TAG, "Đã đạt tốc độ tối thiểu (%d/%d)",
                     s_speed, TREADMILL_SPEED_MAX);
        }
        break;

    default:
        ESP_LOGW(TAG, "Lệnh không xác định: 0x%02X", cmd);
        break;
    }
}

bool treadmill_is_running(void)
{
    return s_running;
}

uint8_t treadmill_get_speed(void)
{
    return s_speed;
}
