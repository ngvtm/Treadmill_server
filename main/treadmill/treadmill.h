#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Protocol command bytes — dùng chung cả 2 phía ---- */
#define TREADMILL_CMD_OFF        0x00
#define TREADMILL_CMD_ON         0x01
#define TREADMILL_CMD_SPEED_UP   0x02
#define TREADMILL_CMD_SPEED_DOWN 0x03

/* FIX: SPEED_MIN = 0 (không phải 1) để tránh mâu thuẫn state với motor */
#define TREADMILL_SPEED_MIN  0
#define TREADMILL_SPEED_MAX  10
#define TREADMILL_SPEED_DEFAULT 3   /* Tốc độ mặc định khi bật máy */

void    treadmill_init(void);
void    treadmill_handle_command(uint8_t cmd);
bool    treadmill_is_running(void);
uint8_t treadmill_get_speed(void);

#ifdef __cplusplus
}
#endif
