// Host unit tests for the motor calibration table.
// Build & run:
//   g++ -std=c++17 -Wall -I../main -o test_motor_calibration test_motor_calibration.cpp && ./test_motor_calibration
#include <cstdio>
#include "motor/motor_calibration.h"

static int failures = 0;
static int checks = 0;

#define EXPECT(cond, name) do { \
    checks++; \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
        failures++; \
    } \
} while (0)

// Legacy linear formula the table defaults must reproduce:
// duty_min = 20% of 1023, then integer ramp (from the old motor.c,
// including its speed==0 special case).
static uint32_t legacy_duty(uint8_t speed)
{
    if (speed == 0) return 0;
    if (speed > 10) speed = 10;
    uint32_t duty_min = (uint32_t)(0.20f * 1023);
    return duty_min + (1023 - duty_min) * speed / 10;
}

int main(void)
{
    EXPECT(MOTOR_CAL_LEVELS == 11, "table covers levels 0..10");

    for (uint8_t s = 0; s <= 10; s++) {
        EXPECT(motor_cal_duty_for_speed(s) == legacy_duty(s),
               "default table reproduces legacy formula");
    }

    // Monotonic non-decreasing, strictly increasing while running.
    for (int s = 1; s < MOTOR_CAL_LEVELS; s++) {
        EXPECT(MOTOR_DUTY_TABLE[s] >= MOTOR_DUTY_TABLE[s - 1],
               "table is non-decreasing");
        if (s >= 2) {
            EXPECT(MOTOR_DUTY_TABLE[s] > MOTOR_DUTY_TABLE[s - 1],
                   "running levels strictly increase");
        }
    }

    EXPECT(MOTOR_DUTY_TABLE[0] == 0, "level 0 = motor off");
    EXPECT(MOTOR_DUTY_TABLE[10] == 1023, "level 10 = full duty");

    for (int s = 0; s < MOTOR_CAL_LEVELS; s++) {
        EXPECT(MOTOR_DUTY_TABLE[s] <= 1023, "duty within 10-bit range");
    }

    EXPECT(motor_cal_duty_for_speed(200) == MOTOR_DUTY_TABLE[10],
           "out-of-range speed clamps to max level");

    printf("%s: %d checks, %d failures\n",
           failures == 0 ? "ALL PASS" : "FAILED", checks, failures);
    return failures == 0 ? 0 : 1;
}
