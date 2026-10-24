#include "settings.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define TAG "SETTINGS"
#define SETTINGS_NVS_NAMESPACE "treadmill_cfg"

static const char *SETTINGS_KEYS[SETTINGS_FEATURE_COUNT] = {
    "ir_enabled",
    "reed_enabled",
};

static uint8_t s_off_mask = 0;  // all enabled by default

void settings_init(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed (%s): all features default ON",
                 esp_err_to_name(err));
        s_off_mask = 0;
        return;
    }
    uint8_t mask = 0;
    for (uint8_t id = 0; id < SETTINGS_FEATURE_COUNT; id++) {
        uint8_t enabled = 1;
        err = nvs_get_u8(handle, SETTINGS_KEYS[id], &enabled);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            enabled = 1;  // never default a safety feature to OFF
        } else if (err != ESP_OK) {
            ESP_LOGW(TAG, "read %s failed (%s): default ON",
                     SETTINGS_KEYS[id], esp_err_to_name(err));
            enabled = 1;
        }
        if (!enabled) {
            mask = settings_mask_set_off(mask, id);
        }
    }
    nvs_close(handle);
    s_off_mask = mask;
    ESP_LOGI(TAG, "loaded: off_mask=0x%02X (ir=%s reed=%s)", s_off_mask,
             settings_is_enabled(SETTINGS_FEATURE_IR) ? "ON" : "OFF",
             settings_is_enabled(SETTINGS_FEATURE_REED) ? "ON" : "OFF");
}

bool settings_is_enabled(uint8_t feature_id)
{
    if (!settings_feature_valid(feature_id)) {
        return true;
    }
    return !settings_mask_is_off(s_off_mask, feature_id);
}

uint8_t settings_get_off_mask(void)
{
    return s_off_mask;
}

bool settings_set_enabled(uint8_t feature_id, bool enable)
{
    if (!settings_feature_valid(feature_id)) {
        return false;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed (%s): change refused",
                 esp_err_to_name(err));
        return false;
    }
    err = nvs_set_u8(handle, SETTINGS_KEYS[feature_id], enable ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "persist %s=%d failed (%s)",
                 SETTINGS_KEYS[feature_id], enable ? 1 : 0,
                 esp_err_to_name(err));
        return false;
    }
    s_off_mask = enable
        ? settings_mask_set_on(s_off_mask, feature_id)
        : settings_mask_set_off(s_off_mask, feature_id);
    ESP_LOGI(TAG, "EVENT: feature %u (%s) -> %s, off_mask=0x%02X",
             feature_id, SETTINGS_KEYS[feature_id],
             enable ? "ON" : "OFF", s_off_mask);
    return true;
}
