#pragma once

// Pure, hardware-independent logic for the reed-switch safety key.
// No ESP-IDF / FreeRTOS dependencies so it can be unit-tested on host
// (g++), following the same pattern as ir_safety_logic.h.
//
// Physical model: a magnet puck is clipped to the runner's clothing
// and tethered by a cord to the reed dock on the machine. The reed
// contact is wired so that "magnet docked" = contact CLOSED. Any of
// {puck pulled off by a fall, cord cut, connector unplugged, reed
// broken} reads as contact OPEN = key absent. Fail-safe by wiring.
//
// Timing model (asymmetric, like the zone logic should be):
//  - Losing the key is confirmed FAST (absent_confirm_ms, default
//    30 ms): just long enough to reject contact bounce / vibration
//    chatter, short enough to stay inside the stop-latency budget.
//  - Gaining the key is confirmed SLOWLY (present_stable_ms,
//    default 100 ms of continuously closed contact).
// A trip LATCHES: re-docking the magnet does not clear it. Only an
// explicit acknowledge (the deliberate ON command path) clears it,
// and only while the key is confirmed present.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SK_EVENT_NONE             0u
#define SK_EVENT_PRESENT          (1u << 0)  // key confirmed present
#define SK_EVENT_TRIPPED          (1u << 1)  // key lost after being present -> latched trip

typedef struct {
    bool     raw_closed;        // last raw sample: contact closed (magnet near)
    uint32_t raw_since_ms;      // timestamp of the last raw change
    bool     present;           // debounced: key confirmed docked
    bool     tripped;           // latched trip, cleared only by acknowledge
} safety_key_logic_t;

static inline void safety_key_logic_init(safety_key_logic_t *l)
{
    l->raw_closed   = false;
    l->raw_since_ms = 0;
    l->present      = false;
    l->tripped      = false;
}

// Feed one raw sample. Returns an SK_EVENT_* bitmask of what changed
// on this sample.
static inline uint32_t safety_key_logic_update(safety_key_logic_t *l,
                                               bool raw_closed,
                                               uint32_t now_ms,
                                               uint32_t present_stable_ms,
                                               uint32_t absent_confirm_ms)
{
    uint32_t events = SK_EVENT_NONE;

    if (raw_closed != l->raw_closed) {
        l->raw_closed   = raw_closed;
        l->raw_since_ms = now_ms;
    }
    uint32_t stable_ms = now_ms - l->raw_since_ms;

    if (raw_closed && !l->present && stable_ms >= present_stable_ms) {
        l->present = true;
        events |= SK_EVENT_PRESENT;
    }

    if (!raw_closed && l->present && stable_ms >= absent_confirm_ms) {
        l->present = false;
        if (!l->tripped) {
            l->tripped = true;
            events |= SK_EVENT_TRIPPED;
        }
    }

    return events;
}

// Acknowledge a latched trip. Succeeds only while the key is
// confirmed present again; the caller treats the acknowledge itself
// as a deliberate user action (e.g. pressing ON on the smartkey).
static inline bool safety_key_logic_acknowledge(safety_key_logic_t *l)
{
    if (!l->tripped) {
        return true;  // nothing to clear
    }
    if (!l->present) {
        return false;
    }
    l->tripped = false;
    return true;
}

#ifdef __cplusplus
}
#endif
