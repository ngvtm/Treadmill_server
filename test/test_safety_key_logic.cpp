// Host unit tests for the reed safety-key pure logic.
// Build & run:
//   g++ -std=c++17 -Wall -I../main -o test_safety_key_logic test_safety_key_logic.cpp && ./test_safety_key_logic
#include <cstdio>
#include "safety_key/safety_key_logic.h"

static int failures = 0;
static int checks = 0;

#define EXPECT(cond, name) do { \
    checks++; \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
        failures++; \
    } \
} while (0)

static const uint32_t PRESENT_MS = 100;
static const uint32_t ABSENT_MS  = 30;

// Feed the same raw value from t0..t1 in 5 ms steps; return events
// accumulated over the span.
static uint32_t feed(safety_key_logic_t *l, bool raw, uint32_t t0, uint32_t t1)
{
    uint32_t ev = SK_EVENT_NONE;
    for (uint32_t t = t0; t <= t1; t += 5) {
        ev |= safety_key_logic_update(l, raw, t, PRESENT_MS, ABSENT_MS);
    }
    return ev;
}

int main(void)
{
    // 1. Boot with no key: never present, never trips, acknowledge is
    //    trivially fine (nothing latched).
    {
        safety_key_logic_t l; safety_key_logic_init(&l);
        uint32_t ev = feed(&l, false, 0, 500);
        EXPECT(ev == SK_EVENT_NONE, "boot absent: no events");
        EXPECT(!l.present && !l.tripped, "boot absent: not present, not tripped");
    }

    // 2. Dock the key: present only after the full stable window.
    {
        safety_key_logic_t l; safety_key_logic_init(&l);
        feed(&l, true, 0, 95);
        EXPECT(!l.present, "dock: not present before stable window");
        uint32_t ev = feed(&l, true, 100, 200);
        EXPECT(l.present, "dock: present after stable window");
        EXPECT(ev & SK_EVENT_PRESENT, "dock: PRESENT event fired");
    }

    // 3. Vibration chatter (10 ms open blips) must NOT trip.
    {
        safety_key_logic_t l; safety_key_logic_init(&l);
        feed(&l, true, 0, 200);
        uint32_t t = 200;
        for (int i = 0; i < 20; i++) {
            feed(&l, false, t, t + 10); t += 10;
            feed(&l, true,  t, t + 40); t += 40;
        }
        EXPECT(l.present && !l.tripped, "chatter: no false trip");
    }

    // 4. Real pull: trips only after the absent-confirm window,
    //    exactly one TRIPPED event, stays latched after re-docking.
    {
        safety_key_logic_t l; safety_key_logic_init(&l);
        feed(&l, true, 0, 200);
        feed(&l, false, 200, 225);
        EXPECT(!l.tripped, "pull: not tripped inside confirm window");
        uint32_t ev = feed(&l, false, 230, 400);
        EXPECT(l.tripped && !l.present, "pull: tripped after confirm window");
        EXPECT(ev & SK_EVENT_TRIPPED, "pull: TRIPPED event fired");
        ev = feed(&l, false, 400, 600);
        EXPECT(!(ev & SK_EVENT_TRIPPED), "pull: TRIPPED fires once");

        // Acknowledge while the key is still out must fail.
        EXPECT(!safety_key_logic_acknowledge(&l), "ack refused while key out");
        EXPECT(l.tripped, "still tripped after refused ack");

        // Re-dock: present comes back, latch stays until acknowledge.
        feed(&l, true, 600, 800);
        EXPECT(l.present && l.tripped, "re-dock: present but still latched");
        EXPECT(safety_key_logic_acknowledge(&l), "ack succeeds with key docked");
        EXPECT(!l.tripped, "ack clears the latch");
    }

    // 5. Pull before ever being docked: no trip (nothing to protect
    //    yet); the ON gate relies on is_present() for that case.
    {
        safety_key_logic_t l; safety_key_logic_init(&l);
        feed(&l, true, 0, 50);            // not long enough to confirm
        uint32_t ev = feed(&l, false, 50, 500);
        EXPECT(!(ev & SK_EVENT_TRIPPED) && !l.tripped, "never-docked removal: no trip");
    }

    // 6. Second fall after acknowledge trips again (latch re-arms).
    {
        safety_key_logic_t l; safety_key_logic_init(&l);
        feed(&l, true, 0, 200);
        feed(&l, false, 200, 300);
        feed(&l, true, 300, 500);
        EXPECT(safety_key_logic_acknowledge(&l), "first ack ok");
        uint32_t ev = feed(&l, false, 500, 700);
        EXPECT(l.tripped && (ev & SK_EVENT_TRIPPED), "second pull trips again");
    }

    printf("%s: %d checks, %d failures\n",
           failures == 0 ? "ALL PASS" : "FAILED", checks, failures);
    return failures == 0 ? 0 : 1;
}
