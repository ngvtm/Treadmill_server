#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 6 IR sensors, active LOW (detect = 0), arranged from front to rear:
 *
 *   IR1, IR2 -> SAFE
 *   IR3      -> WARNING_1: warn only
 *   IR4      -> WARNING_2: reduce speed
 *   IR5, IR6 -> DANGER: controlled fast stop
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
bool ir_safety_is_safe(void);

#ifdef __cplusplus
}
#endif
