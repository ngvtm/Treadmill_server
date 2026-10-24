// Fault-injection scenarios (GĐ4): each scenario injects one fault
// into the pure safety layers and asserts that EVERY layer fails
// closed on its own — no layer relies on another layer to catch the
// fault. These compose modules that are unit-tested separately; the
// value here is the composed verdict under a timeline of faults.
// Build: g++ -std=c++17 -Wall -I../main -o /tmp/test_fault_injection test_fault_injection.cpp

#include <cstdio>
#include "../main/ir_safety/ir_safety_logic.h"
#include "../main/ir_safety/ir_safety.h"
#include "../main/safety_key/safety_key_logic.h"
#include "../main/motor_feedback/motor_feedback_logic.h"
#include "../main/settings/settings_logic.h"
#include "../main/blackbox/blackbox_logic.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond) do {                                             \
    g_checks++;                                                      \
    if (!(cond)) {                                                   \
        g_failures++;                                                \
        std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__);      \
    }                                                                \
} while (0)

// 8 sensors, 2 per role, all healthy + runner in SAFE at 500 mm.
static void healthy_array(ir_sensor_reading_t s[8])
{
    ir_role_t roles[8] = { IR_ROLE_SAFE, IR_ROLE_SAFE,
                           IR_ROLE_WARNING_1, IR_ROLE_WARNING_1,
                           IR_ROLE_WARNING_2, IR_ROLE_WARNING_2,
                           IR_ROLE_DANGER, IR_ROLE_DANGER };
    for (int i = 0; i < 8; i++) {
        s[i].role = roles[i];
        s[i].enabled = true;
        s[i].active = true;
        s[i].status = 0;  // valid
        s[i].distance_mm = (i < 2) ? 500 : 2000;  // only SAFE sees the runner
    }
}

int main(void)
{
    std::printf("fault injection:\n");

    // F1: one VL53 dies mid-run (timeout status). Zone fails closed
    // to DANGER instantly through the filter, and the fault latch
    // sets after 5 blind snapshots and stays set on its own.
    {
        ir_sensor_reading_t s[8];
        healthy_array(s);
        CHECK(ir_logic_effective_zone(s, 8, 700) == IR_ZONE_SAFE);
        s[3].status = 255;  // timeout: blind
        CHECK(ir_logic_effective_zone(s, 8, 700) == IR_ZONE_DANGER);
        ir_zone_filter_t f;
        ir_zone_filter_init(&f);
        ir_zone_filter_cfg_t cfg = { 500, 400 };
        CHECK(ir_zone_filter_update(&f, IR_ZONE_SAFE, 0, &cfg) == IR_ZONE_SAFE);
        CHECK(ir_zone_filter_update(&f, IR_ZONE_DANGER, 100, &cfg) == IR_ZONE_DANGER);
        ir_fault_latch_t latch;
        ir_fault_latch_init(&latch);
        for (int i = 0; i < 4; i++) {
            CHECK(!ir_fault_latch_update(&latch, true, 5));
        }
        CHECK(ir_fault_latch_update(&latch, true, 5));
        // Sensor still blind: acknowledge refuses, latch holds.
        CHECK(!ir_fault_latch_acknowledge(&latch, true));
        CHECK(latch.latched);
        // Sensor healthy again: acknowledge clears, ON allowed again.
        CHECK(ir_fault_latch_acknowledge(&latch, false));
        CHECK(!latch.latched);
    }

    // F2: whole I2C bus dies — every sample ages out past the window.
    // Window = max(500 ms, 4x budget); with the 50 ms running budget
    // that is 500 ms, then the sensor reads blind (254).
    {
        ir_sample_age_t age;
        ir_sample_age_init(&age);
        uint8_t st = ir_sample_age_update(&age, true, 0, 0, 500);
        CHECK(st == 0);  // fresh valid sample
        st = ir_sample_age_update(&age, false, 0, 500, 500);
        CHECK(st == 0);  // exactly at the window: still the last sample
        st = ir_sample_age_update(&age, false, 0, 501, 500);
        CHECK(st == 254);  // past the window: blind
    }

    // F3: belt jam while running — encoder silent, current high.
    // STALL confirms at 2 s; ON stays refused through the 10 s
    // cooldown even after the jam clears.
    {
        motor_feedback_state_t st;
        motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        motor_fault_kind_t f = MOTOR_FAULT_NONE;
        // 1000 mA: above the 500 mA stall floor, below the 2500 mA
        // overcurrent trip — a jam, not a short.
        for (uint32_t t = 0; t <= 1900; t += 100) {
            f = motor_feedback_update(&st, &cfg, true, 3, 0, 1000, t);
            CHECK(f == MOTOR_FAULT_NONE);
        }
        f = motor_feedback_update(&st, &cfg, true, 3, 0, 1000, 2000);
        CHECK(f == MOTOR_FAULT_STALL);
        // Jam cleared at 3 s, but the latch holds until 12 s.
        f = motor_feedback_update(&st, &cfg, false, 0, 10, 100, 3000);
        CHECK(f == MOTOR_FAULT_STALL);
        f = motor_feedback_update(&st, &cfg, false, 0, 10, 100, 11999);
        CHECK(f == MOTOR_FAULT_STALL);
        f = motor_feedback_update(&st, &cfg, false, 0, 10, 100, 12000);
        CHECK(f == MOTOR_FAULT_NONE);
    }

    // F4: driver short while STOPPED — overcurrent still trips
    // (the only fault checked with no command), after 500 ms.
    {
        motor_feedback_state_t st;
        motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        motor_fault_kind_t f = motor_feedback_update(&st, &cfg, false, 0, 0, 4000, 0);
        CHECK(f == MOTOR_FAULT_NONE);
        f = motor_feedback_update(&st, &cfg, false, 0, 0, 4000, 499);
        CHECK(f == MOTOR_FAULT_NONE);
        f = motor_feedback_update(&st, &cfg, false, 0, 0, 4000, 500);
        CHECK(f == MOTOR_FAULT_OVERCURRENT);
    }

    // F5: reed puck pulled mid-run — trip after 30 ms absent, latch
    // holds after re-docking until a deliberate ON acknowledges.
    {
        safety_key_logic_t k;
        safety_key_logic_init(&k);
        uint32_t ev = safety_key_logic_update(&k, true, 0, 100, 30);
        CHECK(!(ev & SK_EVENT_PRESENT));
        ev = safety_key_logic_update(&k, true, 100, 100, 30);
        CHECK(ev & SK_EVENT_PRESENT);
        ev = safety_key_logic_update(&k, false, 110, 100, 30);
        CHECK(!(ev & SK_EVENT_TRIPPED));  // 10 ms: not confirmed yet
        ev = safety_key_logic_update(&k, false, 140, 100, 30);
        CHECK(ev & SK_EVENT_TRIPPED);
        CHECK(k.tripped);
        // Re-dock: latch stays until acknowledged by deliberate ON.
        safety_key_logic_update(&k, true, 300, 100, 30);
        CHECK(k.tripped);
    }

    // F6: settings cannot mask a motor fault — disabling IR changes
    // only the zone gate; the motor-fault layer is independent.
    {
        uint8_t mask = 0;
        mask = settings_mask_set_off(mask, SETTINGS_FEATURE_IR);
        CHECK(settings_mask_is_off(mask, SETTINGS_FEATURE_IR));
        CHECK(!settings_mask_is_off(mask, SETTINGS_FEATURE_REED));
        // The motor layer has no settings input at all: a stall still
        // trips with IR "off" in the mask.
        motor_feedback_state_t st;
        motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        motor_fault_kind_t f = MOTOR_FAULT_NONE;
        for (uint32_t t = 0; t <= 2000; t += 100) {
            f = motor_feedback_update(&st, &cfg, true, 3, 0, 1000, t);
        }
        CHECK(f == MOTOR_FAULT_STALL);
    }

    // F7: two faults at once — neither hides the other.
    {
        ir_sensor_reading_t s[8];
        healthy_array(s);
        s[0].status = 255;
        CHECK(ir_logic_effective_zone(s, 8, 700) == IR_ZONE_DANGER);
        safety_key_logic_t k;
        safety_key_logic_init(&k);
        safety_key_logic_update(&k, true, 0, 100, 30);
        safety_key_logic_update(&k, true, 100, 100, 30);  // present confirmed
        safety_key_logic_update(&k, false, 110, 100, 30);
        uint32_t ev = safety_key_logic_update(&k, false, 140, 100, 30);
        CHECK(ev & SK_EVENT_TRIPPED);
        CHECK(ir_logic_effective_zone(s, 8, 700) == IR_ZONE_DANGER);
        CHECK(k.tripped);
    }

    // F8: full recovery — faults clear, cooldown expires, latches
    // acknowledge, and the composed verdict returns to runnable.
    {
        ir_fault_latch_t latch;
        ir_fault_latch_init(&latch);
        for (int i = 0; i < 5; i++) {
            ir_fault_latch_update(&latch, true, 5);
        }
        CHECK(latch.latched);
        CHECK(ir_fault_latch_acknowledge(&latch, false));
        CHECK(!latch.latched);

        ir_sensor_reading_t s[8];
        healthy_array(s);
        CHECK(ir_logic_is_safe_to_run(s, 8, 700));
    }

    std::printf("  %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL PASS: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    return 1;
}
