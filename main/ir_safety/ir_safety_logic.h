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

// A sensor is "blind" when it is expected to work (enabled) but cannot
// currently be trusted: either it never came active / dropped out, or
// its last read did not yield a trustworthy measurement (I2C failure,
// timeout, persistent error statuses like 2 after a bad recover).
// Blind safety sensors must be treated as unsafe, never silently
// ignored. (Phase 1, 1.2 fix: previously an enabled sensor that went
// INACTIVE — init failed, recovery exhausted — was simply skipped by
// the zone logic, leaving a silent coverage hole while the belt kept
// running. Inactive now counts as blind, so the zone fails closed.)
static inline bool ir_logic_is_blind(const ir_sensor_reading_t *s)
{
    return s->enabled && (!s->active || !ir_logic_status_usable(s->status));
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

// ------------------------------------------------------------------
// Per-sensor sample aging (Phase 1, item 1.2).
//
// In continuous mode a poll usually returns "no new sample yet" — the
// sensor is mid-measurement, not broken. The glue therefore keeps the
// previous sample, but only for a bounded window: a sensor that stops
// producing fresh samples (no I2C error, just silence) must age into
// blindness instead of freezing the last distance forever. A fresh
// sample after aging recovers the sample itself; the 1.1 fault latch
// is a separate, stickier state and is NOT cleared by this.
// ------------------------------------------------------------------

typedef struct {
    uint32_t last_fresh_ms;
    uint8_t  last_status;
    bool     has_sample;
} ir_sample_age_t;

static inline void ir_sample_age_init(ir_sample_age_t *a)
{
    a->last_fresh_ms = 0;
    a->last_status   = IR_RANGE_STATUS_TIMEOUT;  // blind until proven otherwise
    a->has_sample    = false;
}

// Returns the status to store for this sensor after one poll cycle.
// fresh=true carries the poll's status; fresh=false keeps the previous
// status until stale_ms elapses without a fresh sample, then 254.
static inline uint8_t ir_sample_age_update(ir_sample_age_t *a,
                                           bool fresh,
                                           uint8_t fresh_status,
                                           uint32_t now_ms,
                                           uint32_t stale_ms)
{
    if (fresh) {
        a->has_sample    = true;
        a->last_fresh_ms = now_ms;
        a->last_status   = fresh_status;
        return fresh_status;
    }
    if (!a->has_sample) {
        return IR_RANGE_STATUS_TIMEOUT;
    }
    if (now_ms - a->last_fresh_ms > stale_ms) {
        a->last_status = IR_RANGE_STATUS_TIMEOUT;
        return IR_RANGE_STATUS_TIMEOUT;
    }
    return a->last_status;
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

// ------------------------------------------------------------------
// Temporal zone filter (Phase 1, item 1.1).
//
// The raw zone is one snapshot and flickers at threshold boundaries.
// The filter below is ASYMMETRIC on purpose:
//   - Escalation is INSTANT: any DANGER sample, or a move to a more
//     severe warning level, is adopted on the very sample that shows it.
//   - De-escalation is SLOW: a less severe zone must be sustained
//     continuously for clean_ms before it is adopted, so a single clean
//     sample in the middle of danger does not restore speed.
//   - NONE (no person) is special: it is a stop condition, but a single
//     NONE sample must NOT stop the belt (gait, one bad sweep). NONE is
//     adopted only after none_confirm_ms of continuous NONE. While NONE
//     is pending, the previous zone stands; any real zone cancels it.
// Times are in milliseconds on a monotonic clock. The filter is
// time-based (not sweep-count-based) on purpose: sweep duration changes
// ~30x between sequential scans now and parallel scans later, so a
// sweep count would mean different real times before/after that change.
// ------------------------------------------------------------------

typedef struct {
    uint32_t clean_ms;         // sustained time required to de-escalate
    uint32_t none_confirm_ms;  // sustained NONE required to adopt NONE
} ir_zone_filter_cfg_t;

typedef struct {
    bool       initialized;
    ir_zone_t  filtered;
    ir_zone_t  pending_zone;   // de-escalation candidate, or IR_ZONE_NONE if none
    uint32_t   pending_since_ms;
    bool       none_pending;
    uint32_t   none_since_ms;
} ir_zone_filter_t;

static inline void ir_zone_filter_init(ir_zone_filter_t *f)
{
    f->initialized      = false;
    f->filtered         = IR_ZONE_NONE;
    f->pending_zone     = IR_ZONE_NONE;
    f->pending_since_ms = 0;
    f->none_pending     = false;
    f->none_since_ms    = 0;
}

// Severity rank for escalation comparisons. NONE is not ranked here:
// it is handled by its own confirm window, never by instant escalation.
static inline int ir_zone_severity(ir_zone_t z)
{
    switch (z) {
    case IR_ZONE_SAFE:      return 0;
    case IR_ZONE_WARNING_1: return 1;
    case IR_ZONE_WARNING_2: return 2;
    case IR_ZONE_DANGER:    return 3;
    default:                return 3;
    }
}

// Feed one raw (effective) zone sample. Returns the filtered zone.
static inline ir_zone_t ir_zone_filter_update(ir_zone_filter_t *f,
                                              ir_zone_t raw,
                                              uint32_t now_ms,
                                              const ir_zone_filter_cfg_t *cfg)
{
    // First sample after boot: adopt immediately, there is no history
    // to be conservative against (the boot sequence already refreshes
    // once before the ON gate reads the zone).
    if (!f->initialized) {
        f->initialized = true;
        f->filtered    = raw;
        return f->filtered;
    }

    // DANGER is always instant, from any previous zone.
    if (raw == IR_ZONE_DANGER) {
        f->filtered     = IR_ZONE_DANGER;
        f->pending_zone = IR_ZONE_NONE;
        f->none_pending = false;
        return f->filtered;
    }

    // NONE: needs its own confirmation window; any real zone cancels it.
    if (raw == IR_ZONE_NONE) {
        if (f->filtered == IR_ZONE_NONE) {
            return f->filtered;
        }
        if (!f->none_pending) {
            f->none_pending  = true;
            f->none_since_ms = now_ms;
            return f->filtered;
        }
        if (now_ms - f->none_since_ms >= cfg->none_confirm_ms) {
            f->filtered     = IR_ZONE_NONE;
            f->none_pending = false;
            f->pending_zone = IR_ZONE_NONE;
        }
        return f->filtered;
    }

    // A real zone cancels a pending NONE.
    f->none_pending = false;

    if (f->filtered == raw) {
        f->pending_zone = IR_ZONE_NONE;
        return f->filtered;
    }

    // Escalation to a more severe warning: instant.
    if (ir_zone_severity(raw) > ir_zone_severity(f->filtered)) {
        f->filtered     = raw;
        f->pending_zone = IR_ZONE_NONE;
        return f->filtered;
    }

    // De-escalation: the candidate zone must hold continuously for
    // clean_ms. Any change of candidate restarts the timer.
    if (f->pending_zone != raw) {
        f->pending_zone     = raw;
        f->pending_since_ms = now_ms;
        return f->filtered;
    }
    if (now_ms - f->pending_since_ms >= cfg->clean_ms) {
        f->filtered     = f->pending_zone;
        f->pending_zone = IR_ZONE_NONE;
    }
    return f->filtered;
}

// ------------------------------------------------------------------
// Sensor-fault latch (Phase 1, item 1.1).
//
// A sensor that goes blind for a few consecutive snapshots is a fault,
// not a glitch: the latch sets and stays set even if the sensor recovers
// on its own. Clearing requires an explicit acknowledge (the deliberate
// ON command path) while the array is healthy again — the same pattern
// as the reed safety-key latch. The latch is evaluated on snapshots,
// not time, because a blind sensor produces no trustworthy samples at
// all: counting snapshots is the honest unit here.
// ------------------------------------------------------------------

typedef struct {
    bool     latched;
    uint32_t blind_streak;  // consecutive snapshots with any safety sensor blind
} ir_fault_latch_t;

static inline void ir_fault_latch_init(ir_fault_latch_t *l)
{
    l->latched      = false;
    l->blind_streak = 0;
}

// Feed one snapshot's "any safety sensor blind" flag. Returns true if
// the latch is set after this snapshot.
static inline bool ir_fault_latch_update(ir_fault_latch_t *l,
                                         bool any_safety_sensor_blind,
                                         uint32_t latch_after_snapshots)
{
    if (any_safety_sensor_blind) {
        l->blind_streak++;
        if (l->blind_streak >= latch_after_snapshots) {
            l->latched = true;
        }
    } else {
        l->blind_streak = 0;
    }
    return l->latched;
}

// Acknowledge a latched fault. Succeeds only while no safety sensor is
// blind right now; a still-blind array keeps the latch (and the ON gate
// keeps refusing) until the hardware is actually healthy again.
static inline bool ir_fault_latch_acknowledge(ir_fault_latch_t *l,
                                              bool any_safety_sensor_blind)
{
    if (!l->latched) {
        return true;
    }
    if (any_safety_sensor_blind) {
        return false;
    }
    l->latched = false;
    return true;
}

#ifdef __cplusplus
}
#endif
