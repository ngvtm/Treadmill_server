#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ir_safety/ir_safety.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol command bytes shared by smartkey and treadmill. */
#define TREADMILL_CMD_OFF        0x00
#define TREADMILL_CMD_ON         0x01
#define TREADMILL_CMD_SPEED_UP   0x02
#define TREADMILL_CMD_SPEED_DOWN 0x03

#define TREADMILL_SPEED_MIN      0
#define TREADMILL_SPEED_MAX      10
#define TREADMILL_SPEED_DEFAULT  3

void    treadmill_init(void);
void    treadmill_handle_command(uint8_t cmd);
void    treadmill_handle_safety_zone(ir_zone_t zone);
void    treadmill_hard_cutoff(void);
bool    treadmill_is_running(void);
uint8_t treadmill_get_speed(void);
uint8_t treadmill_get_requested_speed(void);
bool    treadmill_is_safety_limited(void);

#ifdef __cplusplus
}
#endif
