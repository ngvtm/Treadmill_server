#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 6 IR sensor V183 — active LOW (detect = mức 0)
 * Bố cục từ đầu → cuối băng chạy:
 *
 *   [IR1-14] [IR2-27] [IR3-26] | [IR4-25] [IR5-23] [IR6-32]
 *   |------- SAFE ZONE --------|-------- DANGER ZONE --------|
 *   |        (1/3 đầu)         |         (2/3 sau)           |
 *
 * Logic:
 *   SAFE   → trẻ đứng ở 1/3 đầu, cho phép chạy
 *   DANGER → trẻ lệch ra 2/3 sau, dừng khẩn cấp
 *   NONE   → không detect ai, không cho chạy
 */

typedef enum {
    IR_ZONE_NONE   = 0,
    IR_ZONE_SAFE   = 1,
    IR_ZONE_DANGER = 2,
} ir_zone_t;

void ir_safety_init(void);
ir_zone_t ir_safety_get_zone(void);
bool ir_safety_is_safe(void);

#ifdef __cplusplus
}
#endif
