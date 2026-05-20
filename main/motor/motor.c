#include "motor.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "MOTOR"

static uint8_t     s_speed     = 0;
static motor_dir_t s_direction = MOTOR_DIR_COAST;

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
    s_direction = MOTOR_DIR_COAST;

    ESP_LOGI(TAG, "Initialized: INA=GPIO%d INB=GPIO%d ENA=GPIO%d PWM=%dHz",
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

    case MOTOR_DIR_COAST:
        gpio_set_level(MOTOR_INA_PIN, 0);
        gpio_set_level(MOTOR_INB_PIN, 0);
        ESP_LOGD(TAG, "Direction: COAST");
        break;

    case MOTOR_DIR_BRAKE:
        gpio_set_level(MOTOR_INA_PIN, 1);
        gpio_set_level(MOTOR_INB_PIN, 1);
        ESP_LOGD(TAG, "Direction: BRAKE");
        break;
    }
}

void motor_ramp_to_speed(uint8_t target_speed, uint32_t step_delay_ms)
{
    if (target_speed > MOTOR_SPEED_MAX) {
        target_speed = MOTOR_SPEED_MAX;
    }

    while (s_speed != target_speed) {
        if (s_speed < target_speed) {
            s_speed++;
        } else {
            s_speed--;
        }

        uint32_t duty = speed_to_duty(s_speed);
        ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, duty);
        ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);

        if (s_speed > 0 && s_direction != MOTOR_DIR_FORWARD) {
            motor_set_direction(MOTOR_DIR_FORWARD);
        }

        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }

    if (s_speed == 0) {
        motor_set_direction(MOTOR_DIR_BRAKE);
    }

    ESP_LOGI(TAG, "Ramp complete: speed=%d/%d", s_speed, MOTOR_SPEED_MAX);
}

void motor_set_speed(uint8_t speed)
{
    if (speed > MOTOR_SPEED_MAX) {
        speed = MOTOR_SPEED_MAX;
    }

    if (speed == 0) {
        motor_stop();
        return;
    }

    s_speed = speed;
    motor_set_direction(MOTOR_DIR_FORWARD);

    uint32_t duty = speed_to_duty(speed);
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, duty);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);

    ESP_LOGI(TAG, "Speed: %d/%d duty=%lu/1023", speed, MOTOR_SPEED_MAX, duty);
}

void motor_controlled_stop(uint32_t step_delay_ms)
{
    motor_ramp_to_speed(0, step_delay_ms);
    ESP_LOGI(TAG, "Motor controlled stop");
}

void motor_stop(void)
{
    motor_controlled_stop(MOTOR_STOP_NORMAL_STEP_MS);
}

void motor_hard_cutoff(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, 0);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
    motor_set_direction(MOTOR_DIR_BRAKE);
    s_speed = 0;
    ESP_LOGW(TAG, "Motor HARD CUTOFF");
}

void motor_coast(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, 0);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
    motor_set_direction(MOTOR_DIR_COAST);
    s_speed = 0;
    ESP_LOGI(TAG, "Motor COAST");
}

uint8_t motor_get_speed(void)
{
    return s_speed;
}
