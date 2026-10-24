#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bang calibrate duty theo muc toc do (0..MOTOR_SPEED_MAX), don vi
 * la gia tri duty LEDC 10-bit (0..1023).
 *
 * Ban mac dinh duoi day tai tao CHINH XAC cong thuc tuyen tinh cu
 * (20% -> 100%) nen hanh vi khong doi cho den khi co so lieu do that.
 *
 * Cach calibrate (chi tiet trong docs/CALIBRATION.md): do toc do
 * bang that (km/h) o tung muc, khong tai va co nguoi chay; muc nao
 * nhanh hon muc tieu thi giam duty cua muc do, cham hon thi tang,
 * giu nguyen tac bang don dieu tang. Sua bang nay, khong sua cong
 * thuc - motor.c chi tra bang.
 */
#define MOTOR_CAL_LEVELS 11

static const uint32_t MOTOR_DUTY_TABLE[MOTOR_CAL_LEVELS] = {
    0,    /* muc 0: dung */
    285,  /* muc 1  (~28%) */
    367,  /* muc 2  (~36%) */
    449,  /* muc 3  (~44%) */
    531,  /* muc 4  (~52%) */
    613,  /* muc 5  (~60%) */
    695,  /* muc 6  (~68%) */
    777,  /* muc 7  (~76%) */
    859,  /* muc 8  (~84%) */
    941,  /* muc 9  (~92%) */
    1023, /* muc 10 (100%) */
};

static inline uint32_t motor_cal_duty_for_speed(uint8_t speed)
{
    if (speed >= MOTOR_CAL_LEVELS) {
        speed = MOTOR_CAL_LEVELS - 1;
    }
    return MOTOR_DUTY_TABLE[speed];
}

#ifdef __cplusplus
}
#endif
