#pragma once

// Pure NVS black-box ring buffer (GĐ4, host-testable).
// The treadmill has no app or cloud, so after a fault the only
// witness is this log: a fixed ring of the last 64 safety events,
// persisted in NVS and printed to the serial log at boot. A reboot
// never erases it — a reboot LOOP is itself evidence (repeated BOOT
// events with no ON between them).
//
// Entry layout is deliberately tiny (10 bytes) so the whole ring
// fits one NVS blob. Timestamps are uptime milliseconds (no RTC on
// this board), so they order events within a boot and, combined with
// the BOOT event, across boots.

#include <stdint.h>
#include <stdbool.h>

#define BLACKBOX_CAPACITY 64

// Event types (detail byte meaning in parentheses).
#define BLACKBOX_EVT_BOOT          0  // (0)
#define BLACKBOX_EVT_ON            1  // (requested speed level)
#define BLACKBOX_EVT_OFF           2  // (0)
#define BLACKBOX_EVT_HARD_CUTOFF   3  // (0)
#define BLACKBOX_EVT_REED_TRIP     4  // (0)
#define BLACKBOX_EVT_FAULT_LATCH   5  // (0)
#define BLACKBOX_EVT_MOTOR_FAULT   6  // (fault kind: 1 stall, 2 overcurrent, 3 no-motion)
#define BLACKBOX_EVT_SETTINGS      7  // (feature_id<<4 | enable)
#define BLACKBOX_EVT_ZONE_DANGER   8  // (zone at the time)

typedef struct {
    uint8_t type;
    uint8_t detail;
    uint32_t time_ms;
    uint32_t seq;
} blackbox_entry_t;

typedef struct {
    blackbox_entry_t entries[BLACKBOX_CAPACITY];
    uint8_t head;       // next slot to overwrite (oldest entry)
    uint8_t count;      // entries stored, up to BLACKBOX_CAPACITY
    uint32_t next_seq;  // monotonic sequence, survives NVS save/load
} blackbox_t;

static inline void blackbox_init(blackbox_t *b)
{
    for (int i = 0; i < BLACKBOX_CAPACITY; i++) {
        b->entries[i].type = 0xFF;
        b->entries[i].detail = 0;
        b->entries[i].time_ms = 0;
        b->entries[i].seq = 0;
    }
    b->head = 0;
    b->count = 0;
    b->next_seq = 0;
}

// Push one event; when full, the oldest entry is overwritten.
static inline void blackbox_push(blackbox_t *b, uint8_t type,
                                 uint8_t detail, uint32_t time_ms)
{
    blackbox_entry_t *e = &b->entries[b->head];
    e->type = type;
    e->detail = detail;
    e->time_ms = time_ms;
    e->seq = b->next_seq++;
    b->head = (uint8_t)((b->head + 1) % BLACKBOX_CAPACITY);
    if (b->count < BLACKBOX_CAPACITY) {
        b->count++;
    }
}

// Number of stored entries.
static inline uint8_t blackbox_count(const blackbox_t *b)
{
    return b->count;
}

// Read the i-th oldest entry (0 = oldest). Returns false when out
// of range. Iteration order is chronological, which is what a boot
// dump prints.
static inline bool blackbox_get_oldest(const blackbox_t *b, uint8_t i,
                                       blackbox_entry_t *out)
{
    if (i >= b->count) {
        return false;
    }
    uint8_t start = (uint8_t)((b->head + BLACKBOX_CAPACITY - b->count) %
                              BLACKBOX_CAPACITY);
    *out = b->entries[(uint8_t)((start + i) % BLACKBOX_CAPACITY)];
    return true;
}
