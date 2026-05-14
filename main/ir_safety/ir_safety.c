#include "ir_safety.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "IR_SAFETY"

/* 3 sensor SAFE — 1/3 đầu băng chạy */
#define IR1_GPIO  14
#define IR2_GPIO  27
#define IR3_GPIO  26

/* 3 sensor DANGER — 2/3 sau băng chạy */
#define IR4_GPIO  25
#define IR5_GPIO  23
#define IR6_GPIO  32

static const int SAFE_GPIOS[]   = {IR1_GPIO, IR2_GPIO, IR3_GPIO};
static const int DANGER_GPIOS[] = {IR4_GPIO, IR5_GPIO, IR6_GPIO};

void ir_safety_init(void)
{
    gpio_config_t cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   /* V183 active LOW */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = (1ULL << IR1_GPIO) | (1ULL << IR2_GPIO) |
                        (1ULL << IR3_GPIO) | (1ULL << IR4_GPIO) |
                        (1ULL << IR5_GPIO) | (1ULL << IR6_GPIO),
    };
    gpio_config(&cfg);
    ESP_LOGI(TAG, "Initialized — SAFE:{14,27,26} DANGER:{25,23,32}");
}

ir_zone_t ir_safety_get_zone(void)
{
    /* Kiểm tra DANGER trước — ưu tiên an toàn tuyệt đối */
    for (int i = 0; i < 3; i++) {
        if (gpio_get_level(DANGER_GPIOS[i]) == 0) {
            return IR_ZONE_DANGER;
        }
    }
    for (int i = 0; i < 3; i++) {
        if (gpio_get_level(SAFE_GPIOS[i]) == 0) {
            return IR_ZONE_SAFE;
        }
    }
    return IR_ZONE_NONE;
}

bool ir_safety_is_safe(void)
{
    return ir_safety_get_zone() == IR_ZONE_SAFE;
}
