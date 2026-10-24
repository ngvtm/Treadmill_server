#include "safety_key.h"
#include "safety_key_logic.h"
#include "blackbox/blackbox.h"

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "SAFETY_KEY"

#ifdef CONFIG_TREADMILL_SAFETY_KEY_GPIO
#define SK_GPIO CONFIG_TREADMILL_SAFETY_KEY_GPIO
#else
#define SK_GPIO 23
#endif

#ifdef CONFIG_TREADMILL_SAFETY_KEY_PRESENT_STABLE_MS
#define SK_PRESENT_STABLE_MS CONFIG_TREADMILL_SAFETY_KEY_PRESENT_STABLE_MS
#else
#define SK_PRESENT_STABLE_MS 100
#endif

#ifdef CONFIG_TREADMILL_SAFETY_KEY_ABSENT_CONFIRM_MS
#define SK_ABSENT_CONFIRM_MS CONFIG_TREADMILL_SAFETY_KEY_ABSENT_CONFIRM_MS
#else
#define SK_ABSENT_CONFIRM_MS 30
#endif

#define SK_POLL_MS 5

static safety_key_logic_t   s_logic;
static safety_key_trip_cb_t s_trip_cb = NULL;

static bool read_key_closed(void)
{
    /* Contact closed (magnet docked) pulls the pin to GND. */
    return gpio_get_level(SK_GPIO) == 0;
}

static void safety_key_task(void *arg)
{
    safety_key_logic_init(&s_logic);

    // Phase 1, item 1.3: watchdog-supervised (see app_main).
    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t events = safety_key_logic_update(&s_logic,
                                                  read_key_closed(),
                                                  now_ms,
                                                  SK_PRESENT_STABLE_MS,
                                                  SK_ABSENT_CONFIRM_MS);
        if (events & SK_EVENT_PRESENT) {
            ESP_LOGI(TAG, "safety key docked (GPIO%d)", SK_GPIO);
        }
        if (events & SK_EVENT_TRIPPED) {
            ESP_LOGE(TAG, "SAFETY KEY PULLED - fall interlock tripped");
            blackbox_record(BLACKBOX_EVT_REED_TRIP, 0);
            if (s_trip_cb) {
                s_trip_cb();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SK_POLL_MS));
    }
}

void safety_key_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << SK_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    xTaskCreate(safety_key_task, "safety_key", 3072, NULL, 11, NULL);
    ESP_LOGI(TAG, "reed safety key on GPIO%d (poll %d ms, present>%d ms, absent>%d ms)",
             SK_GPIO, SK_POLL_MS, SK_PRESENT_STABLE_MS, SK_ABSENT_CONFIRM_MS);
}

void safety_key_set_trip_callback(safety_key_trip_cb_t cb)
{
    s_trip_cb = cb;
}

bool safety_key_is_present(void)
{
    return s_logic.present;
}

bool safety_key_is_tripped(void)
{
    return s_logic.tripped;
}

bool safety_key_acknowledge(void)
{
    bool ok = safety_key_logic_acknowledge(&s_logic);
    if (ok) {
        ESP_LOGI(TAG, "safety key trip acknowledged");
    } else {
        ESP_LOGW(TAG, "acknowledge refused: key not present");
    }
    return ok;
}
