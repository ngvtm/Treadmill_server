#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DC motor control via L298N.
 * Speed: 0-10 steps mapped to PWM duty.
 * Direction: FORWARD is the normal treadmill direction.
 */

#define MOTOR_INA_PIN  GPIO_NUM_18
#define MOTOR_INB_PIN  GPIO_NUM_19
#define MOTOR_ENA_PIN  GPIO_NUM_5

#define MOTOR_SPEED_MIN   0
#define MOTOR_SPEED_MAX   10

#define MOTOR_STOP_NORMAL_STEP_MS  200
#define MOTOR_STOP_FAST_STEP_MS    100

/* LEDC config */
#define MOTOR_LEDC_TIMER    LEDC_TIMER_0
#define MOTOR_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_CHANNEL  LEDC_CHANNEL_0
#define MOTOR_LEDC_FREQ_HZ  5000
#define MOTOR_LEDC_DUTY_RES LEDC_TIMER_10_BIT

typedef enum {
    MOTOR_DIR_FORWARD = 0,
    MOTOR_DIR_BACKWARD,
    MOTOR_DIR_COAST,
    MOTOR_DIR_BRAKE,
} motor_dir_t;

void    motor_init(void);
void    motor_set_speed(uint8_t speed);
void    motor_ramp_to_speed(uint8_t target_speed, uint32_t step_delay_ms);
void    motor_controlled_stop(uint32_t step_delay_ms);
void    motor_set_direction(motor_dir_t dir);
void    motor_stop(void);
void    motor_hard_cutoff(void);
void    motor_coast(void);
uint8_t motor_get_speed(void);

#ifdef __cplusplus
}
#endif
