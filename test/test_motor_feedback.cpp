// Host-based unit tests for motor_feedback_logic.h (GĐ2).
// Build: g++ -std=c++17 -Wall -I../main -o /tmp/test_motor_feedback test_motor_feedback.cpp

#include <cstdio>
#include "../main/motor_feedback/motor_feedback_logic.h"

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
    std::printf("motor feedback logic:\n");

    // 1. Healthy running: encoder moving, sane current -> no fault.
    {
        motor_feedback_state_t st; motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        for (uint32_t t = 0; t <= 5000; t += 100) {
            CHECK(motor_feedback_update(&st, &cfg, true, 3, 5, 800, t)
                  == MOTOR_FAULT_NONE);
        }
    }

    // 2. Stall: commanded, encoder silent, current flowing -> STALL
    //    only after the full 2 s window; a single moving window
    //    resets the suspect.
    {
        motor_feedback_state_t st; motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 0, 900, 0)
              == MOTOR_FAULT_NONE);
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 0, 900, 1000)
              == MOTOR_FAULT_NONE);
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 2, 900, 1100)
              == MOTOR_FAULT_NONE);  // one moving window resets
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 0, 900, 1200)
              == MOTOR_FAULT_NONE);  // suspect restarts here
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 0, 900, 3100)
              == MOTOR_FAULT_NONE);  // 1900 ms: not yet
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 0, 900, 3200)
              == MOTOR_FAULT_STALL);
        // Latched through the cooldown even if the jam clears.
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 5, 800, 5000)
              == MOTOR_FAULT_STALL);
        CHECK(motor_feedback_update(&st, &cfg, false, 0, 0, 0, 13199)
              == MOTOR_FAULT_STALL);
        CHECK(motor_feedback_update(&st, &cfg, false, 0, 0, 0, 13200)
              == MOTOR_FAULT_NONE);  // cooldown expired
    }

    // 3. Overcurrent: 500 ms window, checked even while stopped.
    {
        motor_feedback_state_t st; motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        CHECK(motor_feedback_update(&st, &cfg, false, 0, 0, 3000, 0)
              == MOTOR_FAULT_NONE);
        CHECK(motor_feedback_update(&st, &cfg, false, 0, 0, 3000, 499)
              == MOTOR_FAULT_NONE);
        CHECK(motor_feedback_update(&st, &cfg, false, 0, 0, 3000, 500)
              == MOTOR_FAULT_OVERCURRENT);
    }

    // 4. No-motion: commanded, silent encoder, no current.
    {
        motor_feedback_state_t st; motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 0, 50, 0)
              == MOTOR_FAULT_NONE);
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 0, 50, 2000)
              == MOTOR_FAULT_NO_MOTION);
    }

    // 5. Stopped + silent encoder + no current is healthy (not a fault).
    {
        motor_feedback_state_t st; motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        for (uint32_t t = 0; t <= 5000; t += 100) {
            CHECK(motor_feedback_update(&st, &cfg, false, 0, 0, 0, t)
                  == MOTOR_FAULT_NONE);
        }
    }

    // 6. Inrush below the confirm window does not trip.
    {
        motor_feedback_state_t st; motor_feedback_state_init(&st);
        motor_feedback_cfg_t cfg = motor_feedback_default_cfg();
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 5, 4000, 0)
              == MOTOR_FAULT_NONE);
        CHECK(motor_feedback_update(&st, &cfg, true, 3, 5, 800, 200)
              == MOTOR_FAULT_NONE);
    }

    // 7. Belt speed helper.
    {
        // 2 counts in 100 ms, 20 pulses/rev, 2800 mm loop:
        // 20 counts/s = 1 rev/s -> 2800 mm/s.
        CHECK(motor_feedback_belt_speed_mm_s(2, 100, 20, 2800) == 2800);
        CHECK(motor_feedback_belt_speed_mm_s(-2, 100, 20, 2800) == -2800);
        CHECK(motor_feedback_belt_speed_mm_s(2, 0, 20, 2800) == 0);
        CHECK(motor_feedback_belt_speed_mm_s(2, 100, 0, 2800) == 0);
    }

    // 8. INA219 calibration register.
    {
        CHECK(ina219_calibration(100, 100) == 4096);
        CHECK(ina219_calibration(100, 50) == 8192);
        CHECK(ina219_calibration(20, 500) == 4096);
        CHECK(ina219_calibration(0, 100) == 0);
        CHECK(ina219_calibration(100, 0) == 0);
    }

    std::printf("  %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL PASS: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    return 1;
}
