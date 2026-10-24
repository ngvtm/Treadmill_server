#pragma once

// Glue for GĐ2 motor feedback: quadrature-ish encoder via the ESP32
// PCNT peripheral, INA219 current sense on the shared VL53 I2C bus,
// and the 10 Hz heartbeat output for the external ENA interlock.
// Pure fault decisions live in motor_feedback_logic.h; this file owns
// hardware access and the polling cadence. The safety monitor task
// calls motor_feedback_poll() about every 100 ms.

#include <stdint.h>
#include <stdbool.h>
#include "motor_feedback_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

void motor_feedback_init(void);

// One poll step: sample PCNT delta + INA219 current, feed the pure
// state machine, return the latched fault kind (NONE when healthy).
// Safe to call before init (returns NONE).
motor_fault_kind_t motor_feedback_poll(void);

// Latest latched fault (for the treadmill ON gate). Updated by poll.
motor_fault_kind_t motor_feedback_get_fault_kind(void);

// Latest belt-speed estimate in mm/s (0 when unknown). Exposed for
// logs and for the future speed-mismatch check; not a fault input.
int32_t motor_feedback_get_belt_speed_mm_s(void);

// Latest motor current in mA (0 when the INA219 is unavailable).
int32_t motor_feedback_get_current_ma(void);

#ifdef __cplusplus
}
#endif
