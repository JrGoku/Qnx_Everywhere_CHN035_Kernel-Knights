/*
 * MDD3A dual-motor driver over GPIO.
 * M1A=GPIO7, M1B=GPIO8, M2A=GPIO9, M2B=GPIO10. Motor1=left, motor2=right.
 * Polarity confirmed on hardware - forward/backward/left/right all drive
 * the expected direction with the truth table in motor.c.
 *
 * MDD3A only gives 2 logic pins per motor, no separate PWM/enable, so
 * speed control is done here in software: a background thread chops
 * whichever pin is "active" for the current direction. See motor_set_speed().
 */

#ifndef MOTOR_H
#define MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

int motor_init(void);
void motor_deinit(void);

int motor_set_raw(int motor, int a, int b);  /* motor 1/2, a/b = 0 or 1 */

int motor_forward(void);
int motor_backward(void);
int motor_left(void);   /* pivot left */
int motor_right(void);  /* pivot right */
int motor_stop(void);

/* left_dir/right_dir: -1 backward, 0 stop, 1 forward. What parking.c uses. */
int motor_drive(int left_dir, int right_dir);

/* PWM duty 0-100 for both wheels, trim-corrected (see MOTOR_LEFT_TRIM_PCT
 * in motor.c) so equal speed_pct means equal real wheel speed. */
void motor_set_speed(int percent);

/* Untrimmed per-wheel speed, for calibration only - see motor_test. */
void motor_set_speed_lr(int left_percent, int right_percent);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_H */
