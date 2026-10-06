#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 6 safety sensors, arranged from front to rear:
 *
 *   S0, S1 -> SAFE
 *   S2     -> WARNING_1: warn only
 *   S3     -> WARNING_2: reduce speed
 *   S4, S5 -> DANGER: controlled fast stop
 *   NONE     -> no person detected
 */

typedef enum {
    IR_ZONE_NONE = 0,
    IR_ZONE_SAFE,
    IR_ZONE_WARNING_1,
    IR_ZONE_WARNING_2,
    IR_ZONE_DANGER,
} ir_zone_t;

void ir_safety_init(void);
ir_zone_t ir_safety_get_zone(void);
ir_zone_t ir_safety_get_cached_zone(void);
bool ir_safety_is_ready(void);
bool ir_safety_is_safe(void);
void ir_safety_log_levels(void);
void ir_safety_health_check(void);

#ifdef __cplusplus
}
#endif
