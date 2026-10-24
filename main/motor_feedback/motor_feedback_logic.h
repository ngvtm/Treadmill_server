#pragma once

// Pure motor-fault logic for GĐ2 (host-testable, no ESP-IDF).
// Problem it solves (P0-3): the motor runs open-loop — no encoder, no
// current sense — so a jammed belt, a stalled motor, or a dead driver
// was invisible until something smelled. This state machine watches
// three independent symptoms and latches the first confirmed fault:
//
//   OVERCURRENT: motor current above threshold for a continuous
//     window (absorbs start-up inrush). Checked even while stopped:
//     current with no command means a driver short.
//   STALL: belt commanded but encoder silent AND current flowing
//     (the motor is straining against a jam) for a continuous window.
//   NO_MOTION: belt commanded but encoder silent AND no current
//     (open circuit, dead driver, broken wire).
//
// Latching + cooldown: a confirmed fault latches for a cooldown
// window (default 10 s). During the cooldown ON is refused even if
// the symptom clears, so the user cannot hammer ON against a jam;
// after the cooldown the latch clears on its own and a fresh ON
// re-tests under monitoring (a still-jammed belt trips again within
// the confirm window). There is no extra acknowledge input on this
// hardware — the smartkey has three buttons and no menu yet — so a
// time-based cooldown is the honest policy, documented as such.
//
// Speed-mismatch (measured belt speed vs. the speed the level should
// give) is deliberately NOT a fault here: the level->speed table does
// not exist yet (DS400 calibration numbers are still pending from
// bench measurements). The belt-speed helper below exists so that
// check can be added without touching the glue once the table lands.

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTOR_FAULT_NONE = 0,
    MOTOR_FAULT_OVERCURRENT,
    MOTOR_FAULT_STALL,
    MOTOR_FAULT_NO_MOTION,
} motor_fault_kind_t;

typedef struct {
    uint32_t stall_confirm_ms;       // default 2000
    int32_t  stall_current_ma;       // current that proves straining, default 500
    int32_t  overcurrent_ma;         // default 2500 (PLACEHOLDER: calibrate!)
    uint32_t overcurrent_confirm_ms; // default 500
    uint32_t no_motion_confirm_ms;   // default 2000
    int32_t  no_motion_current_ma;   // below this counts as "no current", default 100
    uint32_t fault_cooldown_ms;      // default 10000
} motor_feedback_cfg_t;

static inline motor_feedback_cfg_t motor_feedback_default_cfg(void)
{
    motor_feedback_cfg_t cfg = {
        .stall_confirm_ms = 2000,
        .stall_current_ma = 500,
        .overcurrent_ma = 2500,
        .overcurrent_confirm_ms = 500,
        .no_motion_confirm_ms = 2000,
        .no_motion_current_ma = 100,
        .fault_cooldown_ms = 10000,
    };
    return cfg;
}

typedef struct {
    motor_fault_kind_t latched;
    uint32_t latched_until_ms;
    uint32_t suspect_since_ms;  // 0 = no suspect currently tracked
    motor_fault_kind_t suspect_kind;
} motor_feedback_state_t;

static inline void motor_feedback_init(motor_feedback_state_t *st)
{
    st->latched = MOTOR_FAULT_NONE;
    st->latched_until_ms = 0;
    st->suspect_since_ms = 0;
    st->suspect_kind = MOTOR_FAULT_NONE;
}

// One evaluation step. running/commanded_level come from the motor
// driver state; encoder_delta_counts is the signed PCNT delta since
// the last call (any motion, either direction, counts as motion);
// current_ma is the latest INA219 reading (signed: negative means
// reverse current, use magnitude). Returns the latched fault kind
// (NONE when healthy). Call at a steady cadence (~100 ms); the
// confirm windows are time-based, not call-count-based.
static inline motor_fault_kind_t motor_feedback_update(
    motor_feedback_state_t *st,
    const motor_feedback_cfg_t *cfg,
    bool running,
    uint8_t commanded_level,
    int32_t encoder_delta_counts,
    int32_t current_ma,
    uint32_t now_ms)
{
    // Cooldown: a latched fault holds until its window expires,
    // regardless of what the inputs do meanwhile.
    if (st->latched != MOTOR_FAULT_NONE) {
        if (now_ms < st->latched_until_ms) {
            return st->latched;
        }
        st->latched = MOTOR_FAULT_NONE;
        st->suspect_since_ms = 0;
        st->suspect_kind = MOTOR_FAULT_NONE;
    }

    // What symptom, if any, is present right now? Overcurrent first:
    // it is the only one checked while stopped, and it outranks the
    // others when several coincide.
    motor_fault_kind_t symptom = MOTOR_FAULT_NONE;
    uint32_t confirm_ms = 0;
    int32_t magnitude_ma = current_ma < 0 ? -current_ma : current_ma;
    if (magnitude_ma >= cfg->overcurrent_ma) {
        symptom = MOTOR_FAULT_OVERCURRENT;
        confirm_ms = cfg->overcurrent_confirm_ms;
    } else if (running && commanded_level > 0 && encoder_delta_counts == 0) {
        if (magnitude_ma >= cfg->stall_current_ma) {
            symptom = MOTOR_FAULT_STALL;
            confirm_ms = cfg->stall_confirm_ms;
        } else if (magnitude_ma < cfg->no_motion_current_ma) {
            symptom = MOTOR_FAULT_NO_MOTION;
            confirm_ms = cfg->no_motion_confirm_ms;
        }
    }

    if (symptom == MOTOR_FAULT_NONE) {
        st->suspect_since_ms = 0;
        st->suspect_kind = MOTOR_FAULT_NONE;
        return MOTOR_FAULT_NONE;
    }
    if (st->suspect_kind != symptom) {
        st->suspect_kind = symptom;
        st->suspect_since_ms = now_ms;
        return MOTOR_FAULT_NONE;
    }
    if (now_ms - st->suspect_since_ms >= confirm_ms) {
        st->latched = symptom;
        st->latched_until_ms = now_ms + cfg->fault_cooldown_ms;
        st->suspect_since_ms = 0;
        st->suspect_kind = MOTOR_FAULT_NONE;
        return st->latched;
    }
    return MOTOR_FAULT_NONE;
}

// Belt speed from encoder counts. Returns mm/s, 0 when the inputs
// cannot produce one (zero window, zero pulses/rev, zero loop).
// belt_loop_mm is the full belt loop length, NOT the deck length.
static inline int32_t motor_feedback_belt_speed_mm_s(
    int32_t delta_counts,
    uint32_t window_ms,
    uint32_t pulses_per_rev,
    uint32_t belt_loop_mm)
{
    if (window_ms == 0 || pulses_per_rev == 0 || belt_loop_mm == 0) {
        return 0;
    }
    int64_t num = (int64_t)delta_counts * (int64_t)belt_loop_mm * 1000;
    int64_t den = (int64_t)pulses_per_rev * (int64_t)window_ms;
    return (int32_t)(num / den);
}

// INA219 calibration register for a given shunt and current LSB.
// Cal = trunc(0.04096 / (current_lsb_A * shunt_ohm)). Arguments in
// milli-ohms and micro-amps to stay in integer math; returns the
// 12-bit register value (0 on invalid input).
static inline uint16_t ina219_calibration(uint32_t shunt_mohm,
                                          uint32_t current_lsb_ua)
{
    if (shunt_mohm == 0 || current_lsb_ua == 0) {
        return 0;
    }
    // Cal = trunc(0.04096 / (lsb_A * shunt_ohm))
    //     = trunc(0.04096 / (lsb_ua*1e-6 * shunt_mohm*1e-3))
    //     = trunc(40960000 / (lsb_ua * shunt_mohm)).
    // Sanity: 100 mOhm + 100 uA -> 40960000/10000 = 4096 (the
    // textbook value for the common INA219 module).
    uint64_t denom = (uint64_t)current_lsb_ua * (uint64_t)shunt_mohm;
    uint64_t cal = 40960000ULL / denom;
    if (cal > 0xFFFE) {
        cal = 0xFFFE;
    }
    return (uint16_t)cal;
}
