// Host-based unit tests for settings_logic.h (Phase 2).
// Build: g++ -std=c++17 -Wall -I../main -o /tmp/test_settings test_settings.cpp

#include <cstdio>
#include "../main/settings/settings_logic.h"

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
    std::printf("settings mask logic:\n");

    CHECK(settings_feature_valid(SETTINGS_FEATURE_IR));
    CHECK(settings_feature_valid(SETTINGS_FEATURE_REED));
    CHECK(!settings_feature_valid(SETTINGS_FEATURE_COUNT));
    CHECK(!settings_feature_valid(15));

    uint8_t mask = 0;
    CHECK(!settings_mask_is_off(mask, SETTINGS_FEATURE_IR));
    mask = settings_mask_set_off(mask, SETTINGS_FEATURE_IR);
    CHECK(settings_mask_is_off(mask, SETTINGS_FEATURE_IR));
    CHECK(!settings_mask_is_off(mask, SETTINGS_FEATURE_REED));
    mask = settings_mask_set_off(mask, SETTINGS_FEATURE_REED);
    CHECK(mask == 0x03);
    mask = settings_mask_set_on(mask, SETTINGS_FEATURE_IR);
    CHECK(mask == 0x02);
    CHECK(!settings_mask_is_off(mask, SETTINGS_FEATURE_IR));
    CHECK(settings_mask_is_off(mask, SETTINGS_FEATURE_REED));

    CHECK(settings_change_allowed(false));
    CHECK(!settings_change_allowed(true));

    std::printf("  %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL PASS: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    return 1;
}
