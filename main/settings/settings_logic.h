#pragma once

// Pure settings-mask logic for Phase 2 (host-testable, no ESP-IDF).
// The server owns one bit per safety feature in features_off_mask:
// bit i SET means feature i is currently OFF. The smartkey renders a
// persistent warning whenever the mask is non-zero. NVS stores one
// bool per feature (see settings.c); this header owns only the mask
// algebra and the change rule, so both can be unit-tested.

#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_FEATURE_IR   0  // VL53 zone protection
#define SETTINGS_FEATURE_REED 1  // reed breakaway key (firmware gate only)
#define SETTINGS_FEATURE_COUNT 2

static inline bool settings_feature_valid(uint8_t id)
{
    return id < SETTINGS_FEATURE_COUNT;
}

// Set (turn OFF) or clear (turn ON) one feature's bit.
static inline uint8_t settings_mask_set_off(uint8_t mask, uint8_t id)
{
    return (uint8_t)(mask | (uint8_t)(1u << id));
}

static inline uint8_t settings_mask_set_on(uint8_t mask, uint8_t id)
{
    return (uint8_t)(mask & (uint8_t)~(1u << id));
}

static inline bool settings_mask_is_off(uint8_t mask, uint8_t id)
{
    return (mask & (uint8_t)(1u << id)) != 0;
}

// The one change rule: settings change only while stopped. The
// caller (treadmill_handle_set_feature) enforces this; the NVS layer
// below it does not need to know about running state.
static inline bool settings_change_allowed(bool running)
{
    return !running;
}
