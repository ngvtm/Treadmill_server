#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Module điều khiển motor DC qua L298N.
 * INA=GPIO12, INB=GPIO14, ENA=GPIO27 (PWM LEDC)
 *
 * Tốc độ: 0–10 bậc (map sang duty cycle PWM 0–100%)
 * Chiều: FORWARD mặc định cho băng chạy
 */

#define MOTOR_INA_PIN     GPIO_NUM_12
#define MOTOR_INB_PIN     GPIO_NUM_14
#define MOTOR_ENA_PIN     GPIO_NUM_27

#define MOTOR_SPEED_MIN   0
#define MOTOR_SPEED_MAX   10

/* LEDC config */
#define MOTOR_LEDC_TIMER    LEDC_TIMER_0
#define MOTOR_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_CHANNEL  LEDC_CHANNEL_0
#define MOTOR_LEDC_FREQ_HZ  5000    /* 5kHz PWM */
#define MOTOR_LEDC_DUTY_RES LEDC_TIMER_10_BIT  /* 0–1023 */

typedef enum {
    MOTOR_DIR_FORWARD = 0,
    MOTOR_DIR_BACKWARD,
    MOTOR_DIR_BRAKE,
} motor_dir_t;

void    motor_init(void);
void    motor_set_speed(uint8_t speed);   /* 0–10 */
void    motor_set_direction(motor_dir_t dir);
void    motor_stop(void);                 /* Dừng ngay (brake) */
void    motor_coast(void);               /* Dừng tự do (coast) */
uint8_t motor_get_speed(void);

#ifdef __cplusplus
}
#endif
