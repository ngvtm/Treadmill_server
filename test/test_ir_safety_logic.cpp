// Host-based unit tests for the pure safety logic in ir_safety_logic.h.
// Build & run: see test/README or the g++ command in the task notes.
// No ESP-IDF needed; ir_safety.h enums are the only firmware dependency.

#include <cstdio>
#include <cstring>
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

// Detection threshold in mm, matching VL53_DETECT_MAX_MM in ir_safety.cpp.
// Side-rail cross-scan geometry on an 800x650 mm belt. The PCB seat is tilted
// 77.5 deg from vertical, so the optical axis rises 12.5 deg above horizontal
// and the lowest ray of the 25 deg cone runs parallel to the belt. The far belt
// edge (650 mm horizontal) reads as a 666 mm slant range, so 700 mm covers the
// full width with margin.
static const uint16_t TH = 700;

// Slant range for a horizontal distance across the belt.
// Optical axis rises 12.5 deg => slant = horizontal / cos(12.5 deg)
static uint16_t slant_mm(uint16_t horizontal_mm)
{
    return (uint16_t)((double)horizontal_mm / 0.97630 + 0.5);
}

// Build the 8-sensor configuration currently under test (symmetric 2-2-2-2):
// S0,S1=SAFE, S2,S3=WARNING_1, S4,S5=WARNING_2, S6,S7=DANGER.
static void make_config(ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS])
{
    std::memset(s, 0, sizeof(ir_sensor_reading_t) * IR_SAFETY_MAX_SENSORS);
    s[0].role = IR_ROLE_SAFE;      s[0].enabled = true; s[0].active = true;
    s[1].role = IR_ROLE_SAFE;      s[1].enabled = true; s[1].active = true;
    s[2].role = IR_ROLE_WARNING_1; s[2].enabled = true; s[2].active = true;
    s[3].role = IR_ROLE_WARNING_1; s[3].enabled = true; s[3].active = true;
    s[4].role = IR_ROLE_WARNING_2; s[4].enabled = true; s[4].active = true;
    s[5].role = IR_ROLE_WARNING_2; s[5].enabled = true; s[5].active = true;
    s[6].role = IR_ROLE_DANGER;    s[6].enabled = true; s[6].active = true;
    s[7].role = IR_ROLE_DANGER;    s[7].enabled = true; s[7].active = true;
    for (int i = 0; i < IR_SAFETY_MAX_SENSORS; i++) {
        s[i].status = 255;
        s[i].distance_mm = 0;
    }
}

// Helper: set a sensor to a valid detection at given distance.
static void detect(ir_sensor_reading_t *s, uint16_t dist)
{
    s->status = IR_RANGE_STATUS_VALID;
    s->distance_mm = dist;
}

// Helper: set a sensor to "no person in range" (valid status, saturated).
static void clear(ir_sensor_reading_t *s)
{
    s->status = IR_RANGE_STATUS_VALID;
    s->distance_mm = 8190;
}

// Helper: set a sensor blind (I2C failure).
static void blind(ir_sensor_reading_t *s)
{
    s->status = IR_RANGE_STATUS_I2CFAIL;
    s->distance_mm = 0;
}

// Helper: put every enabled sensor into a healthy "no person" state.
static void clear_all(ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS])
{
    for (int i = 0; i < IR_SAFETY_MAX_SENSORS; i++) {
        if (s[i].enabled) {
            clear(&s[i]);
        }
    }
}

static void test_detection_basics()
{
    std::printf("test_detection_basics\n");
    ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS];
    make_config(s);

    clear_all(s);
    CHECK(!ir_logic_is_detected(&s[1], TH));

    detect(&s[1], 500);
    CHECK(ir_logic_is_detected(&s[1], TH));

    // Exactly at threshold counts; just beyond does not.
    detect(&s[1], TH);
    CHECK(ir_logic_is_detected(&s[1], TH));
    detect(&s[1], TH + 1);
    CHECK(!ir_logic_is_detected(&s[1], TH));

    // Distance 0 with valid status is not a detection.
    detect(&s[1], 0);
    CHECK(!ir_logic_is_detected(&s[1], TH));

    // Blind sensor never fabricates a detection even if distance looks in-range.
    s[1].status = IR_RANGE_STATUS_I2CFAIL;
    s[1].distance_mm = 300;
    CHECK(!ir_logic_is_detected(&s[1], TH));

    // Inactive sensor never detects.
    s[1].status = IR_RANGE_STATUS_VALID;
    s[1].distance_mm = 300;
    s[1].active = false;
    CHECK(!ir_logic_is_detected(&s[1], TH));
}

static void test_zone_priority()
{
    std::printf("test_zone_priority\n");
    ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS];
    make_config(s);
    clear_all(s);

    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_NONE);

    detect(&s[1], 400);  // SAFE
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_SAFE);

    detect(&s[2], 400);  // WARNING_1 outranks SAFE
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_WARNING_1);

    detect(&s[4], 400);  // WARNING_2 outranks WARNING_1
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_WARNING_2);

    // DANGER wins over everything when multiple fire simultaneously.
    detect(&s[6], 400);  // DANGER
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_DANGER);
    detect(&s[7], 400);  // second DANGER sensor
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_DANGER);
}

static void test_blind_sensor_is_unsafe()
{
    std::printf("test_blind_sensor_is_unsafe\n");
    ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS];
    make_config(s);

    // Person standing safely on S1 (SAFE), but a DANGER sensor S7 is blind.
    clear_all(s);
    detect(&s[1], 400);
    blind(&s[7]);

    // The naive zone would be SAFE, but a blind DANGER sensor must fail closed.
    CHECK(ir_logic_any_safety_sensor_blind(s, IR_SAFETY_MAX_SENSORS));
    CHECK(!ir_logic_is_safe_to_run(s, IR_SAFETY_MAX_SENSORS, TH));
    CHECK(ir_logic_effective_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_DANGER);
}

static void test_ready_reevaluated()
{
    std::printf("test_ready_reevaluated\n");
    ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS];
    make_config(s);
    clear_all(s);

    CHECK(ir_logic_is_ready(s, IR_SAFETY_MAX_SENSORS));

    // A sensor going blind at runtime makes the system not-ready.
    blind(&s[4]);
    CHECK(!ir_logic_is_ready(s, IR_SAFETY_MAX_SENSORS));

    // An enabled sensor that never came active also blocks readiness.
    make_config(s);
    clear_all(s);
    s[5].active = false;
    CHECK(!ir_logic_is_ready(s, IR_SAFETY_MAX_SENSORS));
}

static void test_safe_gate()
{
    std::printf("test_safe_gate\n");
    ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS];
    make_config(s);
    clear_all(s);

    // No person => not safe to run (must confirm SAFE zone).
    CHECK(!ir_logic_is_safe_to_run(s, IR_SAFETY_MAX_SENSORS, TH));

    // Person confirmed in SAFE zone, all healthy => safe to run.
    detect(&s[1], 400);
    CHECK(ir_logic_is_safe_to_run(s, IR_SAFETY_MAX_SENSORS, TH));

    // Person steps into WARNING_1 => not safe to run.
    clear(&s[1]);
    detect(&s[2], 400);
    CHECK(!ir_logic_is_safe_to_run(s, IR_SAFETY_MAX_SENSORS, TH));
}

static void test_status_usable_and_blind()
{
    std::printf("test_status_usable_and_blind\n");
    ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS];
    make_config(s);

    // status 0 (valid) and 4 (out-of-range real reading) are usable/not blind.
    CHECK(ir_logic_status_usable(0));
    CHECK(ir_logic_status_usable(4));
    // I2C fail, timeout, and error status 2 are NOT usable.
    CHECK(!ir_logic_status_usable(253));
    CHECK(!ir_logic_status_usable(254));
    CHECK(!ir_logic_status_usable(2));

    // Regression: a sensor stuck at status 2 (seen after a bad relaxed recover)
    // must count as blind, not silently healthy.
    s[4].status = 2;
    s[4].distance_mm = 8191;
    CHECK(ir_logic_is_blind(&s[4]));
    CHECK(!ir_logic_is_ready(s, IR_SAFETY_MAX_SENSORS));

    // A sensor reporting out-of-range (status 4) is healthy, just sees nobody.
    s[4].status = 4;
    s[4].distance_mm = 8190;
    CHECK(!ir_logic_is_blind(&s[4]));
    CHECK(!ir_logic_is_detected(&s[4], TH));
}

static void test_reset_detection()
{
    std::printf("test_reset_detection\n");
    // target dead + default alive => reset happened.
    CHECK(ir_logic_detect_reset(false, true));
    // target alive => no reset regardless of default.
    CHECK(!ir_logic_detect_reset(true, true));
    CHECK(!ir_logic_detect_reset(true, false));
    // both dead => not a reset (total loss, different failure).
    CHECK(!ir_logic_detect_reset(false, false));
}

// Cross-scan geometry: verify the threshold actually covers the belt width and
// that fixed structure beyond the belt does NOT read as a person. This is the
// regression guard for the "zone latches to DANGER forever" failure mode.
static void test_cross_scan_geometry()
{
    std::printf("test_cross_scan_geometry\n");
    ir_sensor_reading_t s[IR_SAFETY_MAX_SENSORS];
    make_config(s);
    clear_all(s);

    // A child anywhere across the 650 mm belt width must be detected.
    detect(&s[1], slant_mm(200));   // near the rail   -> 213 mm
    CHECK(ir_logic_is_detected(&s[1], TH));
    detect(&s[1], slant_mm(325));   // belt centre     -> 346 mm
    CHECK(ir_logic_is_detected(&s[1], TH));
    detect(&s[1], slant_mm(650));   // far belt edge   -> 692 mm
    CHECK(ir_logic_is_detected(&s[1], TH));
    // The far edge must clear the threshold with a little room to spare.
    CHECK(slant_mm(650) < TH);

    // A baseline target strip parked at 780 mm horizontal (830 mm slant) is
    // beyond the threshold, so an empty belt reads as "nobody".
    detect(&s[1], slant_mm(780));
    CHECK(!ir_logic_is_detected(&s[1], TH));

    // Full empty-belt sweep: every sensor sees only the target strip => the zone
    // must be NONE, never a latched DANGER.
    for (int i = 0; i < IR_SAFETY_MAX_SENSORS; i++) {
        if (s[i].enabled) {
            detect(&s[i], slant_mm(780));
        }
    }
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_NONE);
    CHECK(!ir_logic_is_safe_to_run(s, IR_SAFETY_MAX_SENSORS, TH));

    // Regression: with the OLD 900 mm threshold that same empty belt would have
    // read as a person on every sensor, latching the zone to DANGER.
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, 900) == IR_ZONE_DANGER);

    // Child standing in the SAFE slices of an otherwise empty belt.
    clear_all(s);
    detect(&s[0], slant_mm(325));
    detect(&s[1], slant_mm(325));
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_SAFE);
    CHECK(ir_logic_is_safe_to_run(s, IR_SAFETY_MAX_SENSORS, TH));

    // Child drifted back into the rear slices => DANGER.
    clear_all(s);
    detect(&s[6], slant_mm(325));
    detect(&s[7], slant_mm(325));
    CHECK(ir_logic_calculate_zone(s, IR_SAFETY_MAX_SENSORS, TH) == IR_ZONE_DANGER);
}

int main()
{
    test_detection_basics();
    test_zone_priority();
    test_blind_sensor_is_unsafe();
    test_ready_reevaluated();
    test_safe_gate();
    test_status_usable_and_blind();
    test_reset_detection();
    test_cross_scan_geometry();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("TESTS FAILED\n");
    return 1;
}
