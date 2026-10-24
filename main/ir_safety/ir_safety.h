#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 8 safety sensors, arranged from front to rear, symmetric 2-2-2-2
 * (matches SENSOR_ROLE in ir_safety.cpp — an earlier revision of
 * this comment described the old 6-sensor layout):
 *
 *   S0, S1 -> SAFE
 *   S2, S3 -> WARNING_1: warn only
 *   S4, S5 -> WARNING_2: reduce speed
 *   S6, S7 -> DANGER: controlled fast stop
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
// Phase 1, item 1.1: sensor-fault latch (see ir_safety_logic.h). A latched
// fault refuses ON until a deliberate ON with healthy sensors acknowledges it.
bool ir_safety_is_fault_latched(void);
bool ir_safety_acknowledge_fault(void);
void ir_safety_log_levels(void);
void ir_safety_health_check(void);

#ifdef __cplusplus
}
#endif
