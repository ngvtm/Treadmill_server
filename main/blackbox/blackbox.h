#pragma once

// NVS black-box glue (GĐ4). The ring lives in NVS namespace
// "treadmill_log" (key "events") so it survives resets and power
// loss; every record rewrites the blob (events are rare — a few per
// incident at most — so the write cost is irrelevant).
//
// Read path: blackbox_dump() prints the whole ring, oldest first,
// to the serial log. Call it once at boot: after a fault, the user
// powers the board, opens the serial monitor, and the last 64 safety
// events are right there — no app, no extra tool.

#include <stdint.h>
#include "blackbox_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load the ring from NVS (fresh ring when absent/corrupt), record a
// BOOT event, and persist. Call once in app_main, after NVS init.
void blackbox_setup(void);

// Record one event with the current uptime and persist the ring.
void blackbox_record(uint8_t type, uint8_t detail);

// Print the ring oldest-first to the serial log.
void blackbox_dump(void);

#ifdef __cplusplus
}
#endif
