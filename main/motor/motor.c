#include "motor.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define TAG "MOTOR"

static uint8_t     s_speed     = 0;
static motor_dir_t s_direction = MOTOR_DIR_FORWARD;

static uint32_t speed_to_duty(uint8_t speed)
{
    if (speed == 0) return 0;
    if (speed > MOTOR_SPEED_MAX) speed = MOTOR_SPEED_MAX;
    uint32_t duty_min = (uint32_t)(0.20f * 1023);
    uint32_t duty_max = 1023;
    return duty_min + (duty_max - duty_min) * speed / MOTOR_SPEED_MAX;
}

void motor_init(void)
{
    gpio_config_t io_conf = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
        .pin_bit_mask = (1ULL << MOTOR_INA_PIN)
                      | (1ULL << MOTOR_INB_PIN),
    };
    gpio_config(&io_conf);

    gpio_set_level(MOTOR_INA_PIN, 0);
    gpio_set_level(MOTOR_INB_PIN, 0);

    ledc_timer_config_t timer_conf = {
        .speed_mode      = MOTOR_LEDC_MODE,
        .timer_num       = MOTOR_LEDC_TIMER,
        .duty_resolution = MOTOR_LEDC_DUTY_RES,
        .freq_hz         = MOTOR_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {
        .gpio_num   = MOTOR_ENA_PIN,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = MOTOR_LEDC_CHANNEL,
        .timer_sel  = MOTOR_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_conf);

    s_speed     = 0;
    s_direction = MOTOR_DIR_FORWARD;

    ESP_LOGI(TAG, "Initialized — INA=GPIO%d INB=GPIO%d ENA=GPIO%d PWM=%dHz",
             MOTOR_INA_PIN, MOTOR_INB_PIN, MOTOR_ENA_PIN, MOTOR_LEDC_FREQ_HZ);
}

void motor_set_direction(motor_dir_t dir)
{
    s_direction = dir;
    switch (dir) {
    case MOTOR_DIR_FORWARD:
        gpio_set_level(MOTOR_INA_PIN, 1);
        gpio_set_level(MOTOR_INB_PIN, 0);
        ESP_LOGD(TAG, "Direction: FORWARD");
        break;
    case MOTOR_DIR_BACKWARD:
        gpio_set_level(MOTOR_INA_PIN, 0);
        gpio_set_level(MOTOR_INB_PIN, 1);
        ESP_LOGD(TAG, "Direction: BACKWARD");
        break;
    case MOTOR_DIR_BRAKE:
        gpio_set_level(MOTOR_INA_PIN, 0);
        gpio_set_level(MOTOR_INB_PIN, 0);
        ESP_LOGD(TAG, "Direction: BRAKE");
        break;
    }
}

void motor_set_speed(uint8_t speed)
{
    if (speed > MOTOR_SPEED_MAX) speed = MOTOR_SPEED_MAX;

    /* FIX: speed=0 → gọi motor_stop() để brake đúng cách, không chỉ tắt PWM */
    if (speed == 0) {
        motor_stop();
        return;
    }

    s_speed = speed;
    uint32_t duty = speed_to_duty(speed);
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, duty);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);

    ESP_LOGI(TAG, "Speed: %d/%d → duty=%lu/1023 (%.0f%%)",
             speed, MOTOR_SPEED_MAX, duty,
             (float)duty / 1023.0f * 100.0f);
}

void motor_stop(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, 0);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
    motor_set_direction(MOTOR_DIR_BRAKE);
    s_speed = 0;
    ESP_LOGI(TAG, "Motor STOP (brake)");
}

void motor_coast(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, 0);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
    gpio_set_level(MOTOR_INA_PIN, 0);
    gpio_set_level(MOTOR_INB_PIN, 0);
    s_speed = 0;
    ESP_LOGI(TAG, "Motor COAST (free spin)");
}

uint8_t motor_get_speed(void)
{
    return s_speed;
}
