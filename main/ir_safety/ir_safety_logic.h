#pragma once

// Pure, hardware-independent safety decision logic for the VL53L0X array.
// No ESP-IDF / FreeRTOS dependencies so it can be unit-tested on host (g++).
// The firmware layer (ir_safety.cpp) feeds hardware readings into these
// functions and acts on the results; all safety-critical decisions live here.

#include <stddef.h>
#include <stdint.h>
#include "ir_safety.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IR_SAFETY_MAX_SENSORS 8

// VL53L0X readWithStatus() status codes we care about.
#define IR_RANGE_STATUS_VALID   0   // good measurement
#define IR_RANGE_STATUS_I2CFAIL 253 // I2C read failed (sensor blind / reset)
#define IR_RANGE_STATUS_TIMEOUT 254 // measurement timeout (sensor blind)

// Role each sensor plays in the safety model.
typedef enum {
    IR_ROLE_UNUSED = 0,
    IR_ROLE_SAFE,       // person confirmed in the safe standing position
    IR_ROLE_WARNING_1,  // warn only
    IR_ROLE_WARNING_2,  // reduce speed
    IR_ROLE_DANGER,     // controlled fast stop
} ir_role_t;

// One sensor's configuration + latest reading.
typedef struct {
    ir_role_t role;      // safety role; IR_ROLE_UNUSED => ignored entirely
    bool enabled;        // configured to be present
    bool active;         // successfully initialised at boot
    uint8_t status;      // last readWithStatus() status code
    uint16_t distance_mm; // last measured distance
} ir_sensor_reading_t;

// A sensor counts as "detecting a person" only on a VALID status with an
// in-range distance. A blind/failed sensor never fabricates a detection.
static inline bool ir_logic_is_detected(const ir_sensor_reading_t *s,
                                        uint16_t threshold_mm)
{
    return s->active
        && s->status == IR_RANGE_STATUS_VALID
        && s->distance_mm > 0
        && s->distance_mm <= threshold_mm;
}

// A VL53L0X range status is "usable" only when the measurement can be trusted:
// 0 = valid range, 4 = out of range but a real reading (no target in view).
// Everything else (I2C fail 253, timeout 254, and error codes like 1/2/5)
// means the sensor did not produce a trustworthy distance this cycle.
static inline bool ir_logic_status_usable(uint8_t status)
{
    return status == IR_RANGE_STATUS_VALID || status == 4;
}

// A sensor is "blind" when it is expected to work (enabled + active) but its
// last read did not yield a trustworthy measurement. Covers I2C failure,
// timeout, and persistent error statuses (e.g. status 2 after a bad recover).
// Blind safety sensors must be treated as unsafe, never silently ignored.
static inline bool ir_logic_is_blind(const ir_sensor_reading_t *s)
{
    return s->enabled && s->active && !ir_logic_status_usable(s->status);
}

// A sensor reset back to the default I2C address is inferred when its assigned
// (target) address stops responding but the default 0x29 address reappears.
static inline bool ir_logic_detect_reset(bool target_probe_ok,
                                         bool default_probe_ok)
{
    return (!target_probe_ok) && default_probe_ok;
}

// True if any enabled+active sensor with a real safety role is currently blind.
// A blind safety sensor means we have lost coverage and cannot trust the zone.
static inline bool ir_logic_any_safety_sensor_blind(const ir_sensor_reading_t *s,
                                                    size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (s[i].role != IR_ROLE_UNUSED && ir_logic_is_blind(&s[i])) {
            return true;
        }
    }
    return false;
}

// Compute the zone from a snapshot, honouring sensor roles by severity:
// DANGER > WARNING_2 > WARNING_1 > SAFE > NONE. A sensor only contributes to
// its zone when it genuinely detects a person (valid status, in range).
static inline ir_zone_t ir_logic_calculate_zone(const ir_sensor_reading_t *s,
                                                size_t count,
                                                uint16_t threshold_mm)
{
    bool danger = false, warn2 = false, warn1 = false, safe = false;
    for (size_t i = 0; i < count; i++) {
        if (!ir_logic_is_detected(&s[i], threshold_mm)) {
            continue;
        }
        switch (s[i].role) {
        case IR_ROLE_DANGER:    danger = true; break;
        case IR_ROLE_WARNING_2: warn2 = true;  break;
        case IR_ROLE_WARNING_1: warn1 = true;  break;
        case IR_ROLE_SAFE:      safe = true;   break;
        case IR_ROLE_UNUSED:    default:        break;
        }
    }
    if (danger) return IR_ZONE_DANGER;
    if (warn2)  return IR_ZONE_WARNING_2;
    if (warn1)  return IR_ZONE_WARNING_1;
    if (safe)   return IR_ZONE_SAFE;
    return IR_ZONE_NONE;
}

// Runtime readiness: every enabled sensor must be active AND not blind.
// This is re-evaluated on each snapshot, unlike a boot-only flag.
static inline bool ir_logic_is_ready(const ir_sensor_reading_t *s, size_t count)
{
    size_t enabled = 0;
    for (size_t i = 0; i < count; i++) {
        if (!s[i].enabled) {
            continue;
        }
        enabled++;
        if (!s[i].active || ir_logic_is_blind(&s[i])) {
            return false;
        }
    }
    return enabled > 0;
}

// The single safety gate: is it safe for the treadmill to be ON right now?
// Requires the system ready (all enabled sensors healthy), no blind safety
// sensor, and the person confirmed in the SAFE zone. Any blindness or loss of
// the SAFE confirmation fails closed.
static inline bool ir_logic_is_safe_to_run(const ir_sensor_reading_t *s,
                                           size_t count,
                                           uint16_t threshold_mm)
{
    if (!ir_logic_is_ready(s, count)) {
        return false;
    }
    if (ir_logic_any_safety_sensor_blind(s, count)) {
        return false;
    }
    return ir_logic_calculate_zone(s, count, threshold_mm) == IR_ZONE_SAFE;
}

// Zone to report while a safety sensor is blind: treat as DANGER so the
// treadmill performs its controlled fast stop rather than assuming NONE.
static inline ir_zone_t ir_logic_effective_zone(const ir_sensor_reading_t *s,
                                                size_t count,
                                                uint16_t threshold_mm)
{
    if (ir_logic_any_safety_sensor_blind(s, count)) {
        return IR_ZONE_DANGER;
    }
    return ir_logic_calculate_zone(s, count, threshold_mm);
}

#ifdef __cplusplus
}
#endif
