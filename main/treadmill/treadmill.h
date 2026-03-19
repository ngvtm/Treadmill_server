#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Module treadmill phía Peripheral (Server).
 * Nhận command byte từ Central, cập nhật state machine,
 * hiện tại chỉ in log — sau này thay bằng điều khiển GPIO/PWM motor.
 */

/* ---- Protocol command bytes — dùng chung cả 2 phía ---- */
#define TREADMILL_CMD_OFF        0x00
#define TREADMILL_CMD_ON         0x01
#define TREADMILL_CMD_SPEED_UP   0x02
#define TREADMILL_CMD_SPEED_DOWN 0x03

#define TREADMILL_SPEED_MIN  1
#define TREADMILL_SPEED_MAX  10

/** Khởi tạo module (reset state về mặc định: tắt, speed=1) */
void    treadmill_init(void);

/** Nhận và xử lý 1 byte command từ Central */
void    treadmill_handle_command(uint8_t cmd);

bool    treadmill_is_running(void);
uint8_t treadmill_get_speed(void);

#ifdef __cplusplus
}
#endif
