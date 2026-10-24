#pragma once

// Server-side settings database (Phase 2): which safety features are
// enabled, persisted in NVS namespace "treadmill_cfg" (one bool per
// feature: "ir_enabled", "reed_enabled"). Missing key = enabled:
// a factory reset or an erased flash can never boot with safety
// features silently OFF. Read once at boot into RAM; written on
// every accepted change, with an event log line.
//
// Hard boundary, repeated because it matters: disabling reed in
// software removes ONLY the firmware ON gate. The hardware reed
// interlock in series with the ENA PWM path cannot be disabled by
// software — running without the puck additionally needs the dock
// physically jumpered. Disabling IR removes ONLY the zone gate;
// reed, motor-fault and watchdog layers keep running.

#include <stdint.h>
#include <stdbool.h>
#include "settings_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

void settings_init(void);

// Current enable state (RAM copy, defaults all enabled).
bool settings_is_enabled(uint8_t feature_id);

// features_off_mask for the status byte: bit i set = feature i OFF.
uint8_t settings_get_off_mask(void);

// Persist one change (caller must have checked settings_change_allowed
// and feature validity). Returns false on NVS failure.
bool settings_set_enabled(uint8_t feature_id, bool enable);

#ifdef __cplusplus
}
#endif
