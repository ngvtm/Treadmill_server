#include "blackbox.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "BLACKBOX"
#define BLACKBOX_NVS_NAMESPACE "treadmill_log"
#define BLACKBOX_NVS_KEY "events"

static blackbox_t s_box;
static bool s_loaded = false;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void persist(void)
{
    nvs_handle_t handle;
    if (nvs_open(BLACKBOX_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(handle, BLACKBOX_NVS_KEY, &s_box, sizeof(s_box)) == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
}

void blackbox_setup(void)
{
    blackbox_init(&s_box);
    nvs_handle_t handle;
    if (nvs_open(BLACKBOX_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        size_t len = sizeof(s_box);
        if (nvs_get_blob(handle, BLACKBOX_NVS_KEY, &s_box, &len) != ESP_OK ||
            len != sizeof(s_box) || s_box.count > BLACKBOX_CAPACITY) {
            blackbox_init(&s_box);
        }
        nvs_close(handle);
    }
    s_loaded = true;
    blackbox_record(BLACKBOX_EVT_BOOT, 0);
}

void blackbox_record(uint8_t type, uint8_t detail)
{
    if (!s_loaded) {
        return;  // setup has not run yet; early events are dropped, not crashed on
    }
    blackbox_push(&s_box, type, detail, now_ms());
    persist();
}

void blackbox_dump(void)
{
    if (!s_loaded) {
        return;
    }
    ESP_LOGI(TAG, "=== blackbox: %u events (oldest first) ===",
             blackbox_count(&s_box));
    for (uint8_t i = 0; i < blackbox_count(&s_box); i++) {
        blackbox_entry_t e;
        if (!blackbox_get_oldest(&s_box, i, &e)) {
            break;
        }
        ESP_LOGI(TAG, "#%lu t=%lu ms type=%u detail=%u",
                 (unsigned long)e.seq, (unsigned long)e.time_ms,
                 e.type, e.detail);
    }
    ESP_LOGI(TAG, "=== end blackbox ===");
}
