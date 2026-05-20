#include "ir_safety.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "IR_SAFETY"

#define IR1_GPIO 14
#define IR2_GPIO 27
#define IR3_GPIO 26
#define IR4_GPIO 25
#define IR5_GPIO 23
#define IR6_GPIO 32

void ir_safety_init(void)
{
    gpio_config_t cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = (1ULL << IR1_GPIO) | (1ULL << IR2_GPIO) |
                        (1ULL << IR3_GPIO) | (1ULL << IR4_GPIO) |
                        (1ULL << IR5_GPIO) | (1ULL << IR6_GPIO),
    };
    gpio_config(&cfg);

    ESP_LOGI(TAG, "Initialized: SAFE={14,27}, WARNING_1={26}, WARNING_2={25}, DANGER={23,32}");
}

ir_zone_t ir_safety_get_zone(void)
{
    if (gpio_get_level(IR5_GPIO) == 0 || gpio_get_level(IR6_GPIO) == 0) {
        return IR_ZONE_DANGER;
    }

    if (gpio_get_level(IR4_GPIO) == 0) {
        return IR_ZONE_WARNING_2;
    }

    if (gpio_get_level(IR3_GPIO) == 0) {
        return IR_ZONE_WARNING_1;
    }

    if (gpio_get_level(IR1_GPIO) == 0 || gpio_get_level(IR2_GPIO) == 0) {
        return IR_ZONE_SAFE;
    }

    return IR_ZONE_NONE;
}

bool ir_safety_is_safe(void)
{
    return ir_safety_get_zone() == IR_ZONE_SAFE;
}
