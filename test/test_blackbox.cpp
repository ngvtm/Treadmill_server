// Host-based unit tests for blackbox_logic.h (GĐ4).
// Build: g++ -std=c++17 -Wall -I../main -o /tmp/test_blackbox test_blackbox.cpp

#include <cstdio>
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

int main(void)
{
    std::printf("blackbox ring:\n");

    blackbox_t b;
    blackbox_init(&b);
    CHECK(blackbox_count(&b) == 0);

    blackbox_push(&b, BLACKBOX_EVT_BOOT, 0, 100);
    blackbox_push(&b, BLACKBOX_EVT_ON, 3, 200);
    blackbox_push(&b, BLACKBOX_EVT_OFF, 0, 300);
    CHECK(blackbox_count(&b) == 3);
    blackbox_entry_t e;
    CHECK(blackbox_get_oldest(&b, 0, &e) && e.type == BLACKBOX_EVT_BOOT);
    CHECK(blackbox_get_oldest(&b, 1, &e) && e.type == BLACKBOX_EVT_ON &&
          e.detail == 3 && e.time_ms == 200);
    CHECK(blackbox_get_oldest(&b, 2, &e) && e.type == BLACKBOX_EVT_OFF);
    CHECK(!blackbox_get_oldest(&b, 3, &e));

    // Sequence numbers are monotonic across pushes.
    blackbox_entry_t first, last;
    blackbox_get_oldest(&b, 0, &first);
    blackbox_get_oldest(&b, 2, &last);
    CHECK(last.seq == first.seq + 2);

    // Fill past capacity: oldest entries drop, order stays chronological.
    for (int i = 0; i < BLACKBOX_CAPACITY + 10; i++) {
        blackbox_push(&b, BLACKBOX_EVT_ZONE_DANGER, (uint8_t)i, 1000 + i);
    }
    CHECK(blackbox_count(&b) == BLACKBOX_CAPACITY);
    CHECK(blackbox_get_oldest(&b, 0, &e) && e.detail == 10);
    CHECK(blackbox_get_oldest(&b, BLACKBOX_CAPACITY - 1, &e) &&
          e.detail == (uint8_t)(BLACKBOX_CAPACITY + 9));
    // Sequence kept increasing through the wrap (no reset to 0).
    blackbox_get_oldest(&b, 0, &first);
    blackbox_get_oldest(&b, BLACKBOX_CAPACITY - 1, &last);
    CHECK(last.seq == first.seq + BLACKBOX_CAPACITY - 1);

    std::printf("  %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL PASS: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    return 1;
}
