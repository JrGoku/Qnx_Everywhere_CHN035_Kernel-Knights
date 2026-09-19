#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "gpio.h"
#include "motor.h"

#define GPIO_M1A 7
#define GPIO_M1B 8
#define GPIO_M2A 9
#define GPIO_M2B 10

/* (a,b) pairs for {M1A,M1B} and {M2A,M2B} - confirmed correct on hardware */
#define MOTOR1_FORWARD_A  1
#define MOTOR1_FORWARD_B  0
#define MOTOR1_BACKWARD_A 0
#define MOTOR1_BACKWARD_B 1

#define MOTOR2_FORWARD_A  1
#define MOTOR2_FORWARD_B  0
#define MOTOR2_BACKWARD_A 0
#define MOTOR2_BACKWARD_B 1

#define PWM_PERIOD_US 20000  /* 20ms/50Hz - tune if motors buzz or steps feel coarse */

static int fd_m1a = -1, fd_m1b = -1, fd_m2a = -1, fd_m2b = -1;

/* which pin is currently PWM-chopped for each motor, -1 if stopped */
static int m1_driving_fd = -1;
static int m2_driving_fd = -1;

/* separate duty per motor - the two wheels don't spin at identical RPM
 * for the same PWM duty, so equal speed_pct alone made it veer */
static volatile int pwm_duty_m1 = 40; /* left */
static volatile int pwm_duty_m2 = 40; /* right */
static volatile int pwm_thread_running = 0;
static pthread_t pwm_thread_id;

/* trim to compensate for the slower wheel - find via motor_test's
 * "t <left%> <right%>" raw command, then hardcode and rebuild */
#define MOTOR_LEFT_TRIM_PCT  0
#define MOTOR_RIGHT_TRIM_PCT 0

static void *pwm_loop(void *arg)
{
    (void)arg;

    while (pwm_thread_running) {
        int duty1 = pwm_duty_m1;
        if (duty1 < 0) duty1 = 0;
        if (duty1 > 100) duty1 = 100;
        int duty2 = pwm_duty_m2;
        if (duty2 < 0) duty2 = 0;
        if (duty2 > 100) duty2 = 100;

        int on1_us = (PWM_PERIOD_US * duty1) / 100;
        int on2_us = (PWM_PERIOD_US * duty2) / 100;

        int fd1 = m1_driving_fd;
        int fd2 = m2_driving_fd;

        if (fd1 >= 0 && on1_us > 0) gpio_write(fd1, 1);
        if (fd2 >= 0 && on2_us > 0) gpio_write(fd2, 1);

        /* each motor turns off at its own time within the shared period */
        int events[2];
        int n = 0;
        if (fd1 >= 0 && on1_us > 0 && on1_us < PWM_PERIOD_US) events[n++] = on1_us;
        if (fd2 >= 0 && on2_us > 0 && on2_us < PWM_PERIOD_US) events[n++] = on2_us;
        if (n == 2 && events[0] > events[1]) {
            int tmp = events[0]; events[0] = events[1]; events[1] = tmp;
        }

        int elapsed = 0;
        for (int i = 0; i < n; i++) {
            int wait = events[i] - elapsed;
            if (wait > 0) {
                usleep(wait);
                elapsed = events[i];
            }
            if (fd1 >= 0 && on1_us == events[i]) gpio_write(fd1, 0);
            if (fd2 >= 0 && on2_us == events[i]) gpio_write(fd2, 0);
        }

        if (fd1 >= 0 && on1_us == 0) gpio_write(fd1, 0);
        if (fd2 >= 0 && on2_us == 0) gpio_write(fd2, 0);

        if (PWM_PERIOD_US > elapsed) {
            usleep(PWM_PERIOD_US - elapsed);
        }
    }
    return NULL;
}

int motor_init(void)
{
    fd_m1a = gpio_open(GPIO_M1A);
    fd_m1b = gpio_open(GPIO_M1B);
    fd_m2a = gpio_open(GPIO_M2A);
    fd_m2b = gpio_open(GPIO_M2B);

    if (fd_m1a < 0 || fd_m1b < 0 || fd_m2a < 0 || fd_m2b < 0) {
        fprintf(stderr, "motor_init: failed to open one or more motor GPIOs\n");
        return -1;
    }

    if (gpio_set_output(fd_m1a) < 0 || gpio_set_output(fd_m1b) < 0 ||
        gpio_set_output(fd_m2a) < 0 || gpio_set_output(fd_m2b) < 0) {
        fprintf(stderr, "motor_init: failed to set one or more motor GPIOs as output\n");
        return -1;
    }

    if (motor_stop() < 0) {
        return -1;
    }

    pwm_thread_running = 1;
    if (pthread_create(&pwm_thread_id, NULL, pwm_loop, NULL) != 0) {
        fprintf(stderr, "motor_init: failed to start PWM thread\n");
        pwm_thread_running = 0;
        return -1;
    }

    return 0;
}

void motor_deinit(void)
{
    motor_stop();

    if (pwm_thread_running) {
        pwm_thread_running = 0;
        pthread_join(pwm_thread_id, NULL);
    }

    gpio_close(fd_m1a);
    gpio_close(fd_m1b);
    gpio_close(fd_m2a);
    gpio_close(fd_m2b);
    fd_m1a = fd_m1b = fd_m2a = fd_m2b = -1;
}

int motor_set_raw(int motor, int a, int b)
{
    int fda, fdb;

    if (motor == 1) {
        fda = fd_m1a;
        fdb = fd_m1b;
    } else if (motor == 2) {
        fda = fd_m2a;
        fdb = fd_m2b;
    } else {
        fprintf(stderr, "motor_set_raw: invalid motor %d\n", motor);
        return -1;
    }

    int driving_fd = -1;
    if (a && !b) {
        driving_fd = fda;
        if (gpio_write(fdb, 0) < 0) return -1;
    } else if (b && !a) {
        driving_fd = fdb;
        if (gpio_write(fda, 0) < 0) return -1;
    } else {
        /* both 0 (stop) or both 1 (brake, unused) - drive directly */
        if (gpio_write(fda, a) < 0) return -1;
        if (gpio_write(fdb, b) < 0) return -1;
    }

    if (motor == 1) {
        m1_driving_fd = driving_fd;
    } else {
        m2_driving_fd = driving_fd;
    }

    return 0;
}

int motor_forward(void)
{
    if (motor_set_raw(1, MOTOR1_FORWARD_A, MOTOR1_FORWARD_B) < 0) return -1;
    if (motor_set_raw(2, MOTOR2_FORWARD_A, MOTOR2_FORWARD_B) < 0) return -1;
    return 0;
}

int motor_backward(void)
{
    if (motor_set_raw(1, MOTOR1_BACKWARD_A, MOTOR1_BACKWARD_B) < 0) return -1;
    if (motor_set_raw(2, MOTOR2_BACKWARD_A, MOTOR2_BACKWARD_B) < 0) return -1;
    return 0;
}

int motor_left(void)
{
    /* pivot left: left wheel back, right wheel forward */
    if (motor_set_raw(1, MOTOR1_BACKWARD_A, MOTOR1_BACKWARD_B) < 0) return -1;
    if (motor_set_raw(2, MOTOR2_FORWARD_A, MOTOR2_FORWARD_B) < 0) return -1;
    return 0;
}

int motor_right(void)
{
    if (motor_set_raw(1, MOTOR1_FORWARD_A, MOTOR1_FORWARD_B) < 0) return -1;
    if (motor_set_raw(2, MOTOR2_BACKWARD_A, MOTOR2_BACKWARD_B) < 0) return -1;
    return 0;
}

int motor_stop(void)
{
    if (motor_set_raw(1, 0, 0) < 0) return -1;
    if (motor_set_raw(2, 0, 0) < 0) return -1;
    return 0;
}

int motor_drive(int left_dir, int right_dir)
{
    int la, lb, ra, rb;

    if (left_dir > 0) {
        la = MOTOR1_FORWARD_A;  lb = MOTOR1_FORWARD_B;
    } else if (left_dir < 0) {
        la = MOTOR1_BACKWARD_A; lb = MOTOR1_BACKWARD_B;
    } else {
        la = 0; lb = 0;
    }

    if (right_dir > 0) {
        ra = MOTOR2_FORWARD_A;  rb = MOTOR2_FORWARD_B;
    } else if (right_dir < 0) {
        ra = MOTOR2_BACKWARD_A; rb = MOTOR2_BACKWARD_B;
    } else {
        ra = 0; rb = 0;
    }

    if (motor_set_raw(1, la, lb) < 0) return -1;
    if (motor_set_raw(2, ra, rb) < 0) return -1;
    return 0;
}

void motor_set_speed(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    int left  = percent + MOTOR_LEFT_TRIM_PCT;
    int right = percent + MOTOR_RIGHT_TRIM_PCT;
    if (left  < 0) left  = 0;  if (left  > 100) left  = 100;
    if (right < 0) right = 0;  if (right > 100) right = 100;

    pwm_duty_m1 = left;
    pwm_duty_m2 = right;
}

void motor_set_speed_lr(int left_percent, int right_percent)
{
    /* untrimmed - calibration only, normal code uses motor_set_speed() */
    if (left_percent  < 0) left_percent  = 0;  if (left_percent  > 100) left_percent  = 100;
    if (right_percent < 0) right_percent = 0;  if (right_percent > 100) right_percent = 100;

    pwm_duty_m1 = left_percent;
    pwm_duty_m2 = right_percent;
}
