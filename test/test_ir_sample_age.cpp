// Host-based unit tests for the Phase 1 item 1.2 sample-aging helper and
// the inactive-sensor-is-blind fix in ir_safety_logic.h. No ESP-IDF needed.
// Build: g++ -std=c++17 -Wall -I../main -o /tmp/test_ir_sample_age test_ir_sample_age.cpp

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

int main(void)
{
    std::printf("sample aging + inactive-is-blind:\n");

    // 1. Never sampled -> blind (254) from the start.
    {
        ir_sample_age_t a; ir_sample_age_init(&a);
        CHECK(ir_sample_age_update(&a, false, 0, 1000, 500) == 254);
    }

    // 2. Fresh samples pass through and refresh the clock; silence
    //    inside the window keeps the last status; past it -> 254.
    {
        ir_sample_age_t a; ir_sample_age_init(&a);
        CHECK(ir_sample_age_update(&a, true, 0, 1000, 500) == 0);
        CHECK(ir_sample_age_update(&a, false, 0, 1400, 500) == 0);
        CHECK(ir_sample_age_update(&a, false, 0, 1500, 500) == 0);
        CHECK(ir_sample_age_update(&a, false, 0, 1501, 500) == 254);
        // A fresh sample after aging recovers the sample itself
        // (the 1.1 fault latch is separate and stays set).
        CHECK(ir_sample_age_update(&a, true, 0, 2000, 500) == 0);
        CHECK(ir_sample_age_update(&a, false, 0, 2400, 500) == 0);
    }

    // 3. A fresh blind sample (253/254) passes through immediately —
    //    aging never masks a real error as "still fine".
    {
        ir_sample_age_t a; ir_sample_age_init(&a);
        CHECK(ir_sample_age_update(&a, true, 0, 1000, 500) == 0);
        CHECK(ir_sample_age_update(&a, true, 253, 1100, 500) == 253);
        CHECK(ir_sample_age_update(&a, false, 0, 1200, 500) == 253);
    }

    // 4. Inactive but enabled sensor counts as blind (1.2 fix): the old
    //    code silently skipped it in the zone, leaving a coverage hole.
    {
        ir_sensor_reading_t s = {};
        s.role = IR_ROLE_DANGER;
        s.enabled = true;
        s.active = false;
        s.status = IR_RANGE_STATUS_VALID;  // even a "valid" status: inactive wins
        s.distance_mm = 300;
        CHECK(ir_logic_is_blind(&s));
        CHECK(!ir_logic_is_detected(&s, 700));

        ir_sensor_reading_t arr[1] = { s };
        CHECK(ir_logic_any_safety_sensor_blind(arr, 1));
        CHECK(ir_logic_effective_zone(arr, 1, 700) == IR_ZONE_DANGER);
        CHECK(!ir_logic_is_ready(arr, 1));
    }

    // 5. Disabled + inactive is still ignored (not a safety sensor).
    {
        ir_sensor_reading_t s = {};
        s.role = IR_ROLE_UNUSED;
        s.enabled = false;
        s.active = false;
        s.status = IR_RANGE_STATUS_VALID;
        CHECK(!ir_logic_is_blind(&s));
        ir_sensor_reading_t arr[1] = { s };
        CHECK(!ir_logic_any_safety_sensor_blind(arr, 1));
        CHECK(ir_logic_effective_zone(arr, 1, 700) == IR_ZONE_NONE);
    }

    std::printf("  %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL PASS: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    return 1;
}
