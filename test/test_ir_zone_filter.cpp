// Host-based unit tests for the Phase 1 item 1.1 temporal zone filter and
// sensor-fault latch in ir_safety_logic.h. No ESP-IDF needed.
// Build: g++ -std=c++17 -Wall -I../main -o /tmp/test_ir_zone_filter test_ir_zone_filter.cpp

#include <cstdio>
#include "../main/ir_safety/ir_safety_logic.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond) do {                                             \
    g_checks++;                                                      \
    if (!(cond)) {                                                   \
        g_failures++;                                                \
        std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__);      \
    }                                                                \
} while (0)

static const ir_zone_filter_cfg_t CFG = { .clean_ms = 500, .none_confirm_ms = 400 };

static ir_zone_t feed(ir_zone_filter_t *f, ir_zone_t raw, uint32_t t)
{
    return ir_zone_filter_update(f, raw, t, &CFG);
}

int main(void)
{
    std::printf("zone filter + fault latch:\n");

    // 1. First sample after boot is adopted immediately (no history yet).
    {
        ir_zone_filter_t f; ir_zone_filter_init(&f);
        CHECK(feed(&f, IR_ZONE_SAFE, 0) == IR_ZONE_SAFE);
    }

    // 2. Escalation is instant: SAFE -> WARNING_1 -> WARNING_2 -> DANGER,
    //    each on the very sample that shows it.
    {
        ir_zone_filter_t f; ir_zone_filter_init(&f);
        CHECK(feed(&f, IR_ZONE_SAFE, 0) == IR_ZONE_SAFE);
        CHECK(feed(&f, IR_ZONE_WARNING_1, 100) == IR_ZONE_WARNING_1);
        CHECK(feed(&f, IR_ZONE_WARNING_2, 200) == IR_ZONE_WARNING_2);
        CHECK(feed(&f, IR_ZONE_DANGER, 300) == IR_ZONE_DANGER);
    }

    // 3. De-escalation is slow: DANGER -> SAFE blip does NOT restore;
    //    only 500 ms of continuous SAFE does. A flicker back to WARNING_2
    //    restarts the clean timer.
    {
        ir_zone_filter_t f; ir_zone_filter_init(&f);
        CHECK(feed(&f, IR_ZONE_SAFE, 0) == IR_ZONE_SAFE);
        CHECK(feed(&f, IR_ZONE_DANGER, 100) == IR_ZONE_DANGER);
        CHECK(feed(&f, IR_ZONE_SAFE, 200) == IR_ZONE_DANGER);   // pending starts
        CHECK(feed(&f, IR_ZONE_SAFE, 600) == IR_ZONE_DANGER);   // 400 ms < 500
        CHECK(feed(&f, IR_ZONE_WARNING_2, 650) == IR_ZONE_DANGER); // flicker: timer restarts
        CHECK(feed(&f, IR_ZONE_SAFE, 700) == IR_ZONE_DANGER);   // pending restarts at SAFE
        CHECK(feed(&f, IR_ZONE_SAFE, 1199) == IR_ZONE_DANGER);  // 499 ms < 500
        CHECK(feed(&f, IR_ZONE_SAFE, 1200) == IR_ZONE_SAFE);    // 500 ms reached
    }

    // 4. WARNING_2 -> SAFE also needs the clean window (speed is not
    //    restored by one clean sample mid-warning).
    {
        ir_zone_filter_t f; ir_zone_filter_init(&f);
        CHECK(feed(&f, IR_ZONE_WARNING_2, 0) == IR_ZONE_WARNING_2);
        CHECK(feed(&f, IR_ZONE_SAFE, 100) == IR_ZONE_WARNING_2);
        CHECK(feed(&f, IR_ZONE_SAFE, 599) == IR_ZONE_WARNING_2);
        CHECK(feed(&f, IR_ZONE_SAFE, 600) == IR_ZONE_SAFE);
    }

    // 5. NONE needs its own 400 ms confirmation: a 300 ms NONE blip
    //    (gait / one bad sweep) never stops the belt; sustained NONE does.
    {
        ir_zone_filter_t f; ir_zone_filter_init(&f);
        CHECK(feed(&f, IR_ZONE_SAFE, 0) == IR_ZONE_SAFE);
        CHECK(feed(&f, IR_ZONE_NONE, 100) == IR_ZONE_SAFE);   // pending starts
        CHECK(feed(&f, IR_ZONE_NONE, 399) == IR_ZONE_SAFE);   // 299 ms < 400
        CHECK(feed(&f, IR_ZONE_SAFE, 450) == IR_ZONE_SAFE);   // cancelled by real zone
        CHECK(feed(&f, IR_ZONE_NONE, 500) == IR_ZONE_SAFE);
        CHECK(feed(&f, IR_ZONE_NONE, 899) == IR_ZONE_SAFE);   // 399 ms < 400
        CHECK(feed(&f, IR_ZONE_NONE, 900) == IR_ZONE_NONE);   // 400 ms reached
    }

    // 6. DANGER during a pending NONE wins immediately (it is a real,
    //    more severe signal, not a gap in detection).
    {
        ir_zone_filter_t f; ir_zone_filter_init(&f);
        CHECK(feed(&f, IR_ZONE_SAFE, 0) == IR_ZONE_SAFE);
        CHECK(feed(&f, IR_ZONE_NONE, 100) == IR_ZONE_SAFE);
        CHECK(feed(&f, IR_ZONE_DANGER, 150) == IR_ZONE_DANGER);
    }

    // 7. Recovery from NONE needs the clean window too (belt stays stopped
    //    until SAFE holds for 500 ms).
    {
        ir_zone_filter_t f; ir_zone_filter_init(&f);
        CHECK(feed(&f, IR_ZONE_NONE, 0) == IR_ZONE_NONE);
        CHECK(feed(&f, IR_ZONE_SAFE, 100) == IR_ZONE_NONE);
        CHECK(feed(&f, IR_ZONE_SAFE, 599) == IR_ZONE_NONE);
        CHECK(feed(&f, IR_ZONE_SAFE, 600) == IR_ZONE_SAFE);
    }

    // 8. Fault latch: 5 consecutive blind snapshots latch; healthy
    //    snapshots do NOT clear it; acknowledge fails while still blind
    //    and succeeds once healthy.
    {
        ir_fault_latch_t l; ir_fault_latch_init(&l);
        for (int i = 0; i < 4; i++) {
            CHECK(!ir_fault_latch_update(&l, true, 5));
        }
        CHECK(ir_fault_latch_update(&l, true, 5));   // 5th blind snapshot latches
        CHECK(ir_fault_latch_update(&l, false, 5));  // recovery alone does NOT clear it
        CHECK(l.latched);
        CHECK(!ir_fault_latch_acknowledge(&l, true));  // still blind: stays latched
        CHECK(ir_fault_latch_acknowledge(&l, false)); // healthy: acknowledged
        CHECK(!l.latched);
        CHECK(ir_fault_latch_acknowledge(&l, false)); // no latch: trivially ok
    }

    std::printf("  %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL PASS: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    return 1;
}
