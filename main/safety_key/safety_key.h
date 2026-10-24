#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reed-switch safety key (breakaway fall protection for the runner).
 *
 * Wiring (fail-safe): reed contact between the configured GPIO and
 * GND; the magnet puck docked CLOSES the contact, pulling the GPIO
 * low through the internal pull-up. Puck pulled off by a fall, cord
 * cut or connector unplugged => contact open => GPIO high => key
 * absent. The same reed contact must ALSO sit in the hardware
 * interlock path of the L298N ENA line (see
 * docs/REED_SAFETY_KEY_DESIGN.md) so the motor is cut even if the
 * firmware hangs; this module is the firmware half: gating ON,
 * hard-cutoff on trip, latching, and reporting.
 *
 * A background task polls the GPIO every 5 ms and feeds the pure
 * logic in safety_key_logic.h. Trips are latched until acknowledged
 * through the deliberate ON path (treadmill_handle_command).
 */

typedef void (*safety_key_trip_cb_t)(void);

void safety_key_init(void);
void safety_key_set_trip_callback(safety_key_trip_cb_t cb);

/** Debounced state: magnet puck confirmed docked. */
bool safety_key_is_present(void);

/** Latched trip state: the key was lost after being present. */
bool safety_key_is_tripped(void);

/** Clear a latched trip; only succeeds while the key is present. */
bool safety_key_acknowledge(void);

#ifdef __cplusplus
}
#endif
