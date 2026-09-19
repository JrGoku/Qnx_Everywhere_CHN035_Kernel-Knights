/*
 * HC-SR04 ultrasonic ranging over rpi_gpio (/dev/gpio/<N>).
 *
 * BCM pin map, sensor IDs 1-8 = US1-US8:
 *   US1: TRIG 6,  ECHO 12       US5: TRIG 21, ECHO 22
 *   US2: TRIG 13, ECHO 16       US6: TRIG 23, ECHO 24
 *   US3: TRIG 17, ECHO 18       US7: TRIG 25, ECHO 26
 *   US4: TRIG 19, ECHO 20       US8: TRIG 14, ECHO 27
 * Physical placement: US1=front, US2=front-right, US3=right,
 * US4=back-right, US5=back, US6=back-left, US7=left, US8=front-left.
 *
 * ECHO is 5V out of the sensor - needs a level shifter down to 3.3V
 * before the Pi GPIO pin, done in hardware, not this code's problem.
 * Sensors fire one at a time, not in parallel, to avoid cross-echo.
 */

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#ifdef __cplusplus
extern "C" {
#endif

#define ULTRASONIC_NUM_SENSORS 8

#define ULTRASONIC_NO_ECHO (-1.0f)  /* no echo within timeout = out of range */

int ultrasonic_init(void);
void ultrasonic_deinit(void);
int ultrasonic_read_cm(int sensor_id, float *distance_cm);  /* sensor_id 1..8 */
int ultrasonic_read_all(float distances[ULTRASONIC_NUM_SENSORS]);  /* [0]=US1.. */

#ifdef __cplusplus
}
#endif

#endif /* ULTRASONIC_H */
