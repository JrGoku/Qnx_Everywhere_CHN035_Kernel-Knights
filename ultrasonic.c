#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "gpio.h"
#include "ultrasonic.h"

/* TRIG/ECHO BCM pin table, indexed 0..7 for US1..US8. */
static const int trig_pins[ULTRASONIC_NUM_SENSORS] = { 6, 13, 17, 19, 21, 23, 25, 14 };
static const int echo_pins[ULTRASONIC_NUM_SENSORS] = { 12, 16, 18, 20, 22, 24, 26, 27 };

static int trig_fd[ULTRASONIC_NUM_SENSORS];
static int echo_fd[ULTRASONIC_NUM_SENSORS];

#define ECHO_WAIT_TIMEOUT_US   30000  /* ~4m round trip, past HC-SR04 range */
#define SPEED_OF_SOUND_CM_PER_US 0.0343f

static long elapsed_us(struct timespec *start, struct timespec *end)
{
    long sec_diff = end->tv_sec - start->tv_sec;
    long nsec_diff = end->tv_nsec - start->tv_nsec;
    return sec_diff * 1000000L + nsec_diff / 1000L;
}

int ultrasonic_init(void)
{
    for (int i = 0; i < ULTRASONIC_NUM_SENSORS; i++) {
        trig_fd[i] = gpio_open(trig_pins[i]);
        echo_fd[i] = gpio_open(echo_pins[i]);

        if (trig_fd[i] < 0 || echo_fd[i] < 0) {
            fprintf(stderr, "ultrasonic_init: failed to open GPIO for sensor US%d\n", i + 1);
            return -1;
        }

        if (gpio_set_output(trig_fd[i]) < 0) {
            fprintf(stderr, "ultrasonic_init: failed to set TRIG US%d as output\n", i + 1);
            return -1;
        }
        if (gpio_write(trig_fd[i], 0) < 0) {
            fprintf(stderr, "ultrasonic_init: failed to init TRIG US%d low\n", i + 1);
            return -1;
        }
        if (gpio_set_input(echo_fd[i]) < 0) {
            fprintf(stderr, "ultrasonic_init: failed to set ECHO US%d as input\n", i + 1);
            return -1;
        }
    }
    return 0;
}

void ultrasonic_deinit(void)
{
    for (int i = 0; i < ULTRASONIC_NUM_SENSORS; i++) {
        gpio_close(trig_fd[i]);
        gpio_close(echo_fd[i]);
        trig_fd[i] = -1;
        echo_fd[i] = -1;
    }
}

int ultrasonic_read_cm(int sensor_id, float *distance_cm)
{
    if (sensor_id < 1 || sensor_id > ULTRASONIC_NUM_SENSORS) {
        fprintf(stderr, "ultrasonic_read_cm: invalid sensor_id %d\n", sensor_id);
        return -1;
    }
    int idx = sensor_id - 1;
    int tfd = trig_fd[idx];
    int efd = echo_fd[idx];

    /* trigger pulse, >=10us */
    if (gpio_write(tfd, 1) < 0) return -1;
    usleep(15);
    if (gpio_write(tfd, 0) < 0) return -1;

    /* wait for echo to go high */
    struct timespec t_wait_start, t_now, t_pulse_start, t_pulse_end;
    clock_gettime(CLOCK_MONOTONIC, &t_wait_start);

    int level;
    for (;;) {
        level = gpio_read(efd);
        if (level < 0) return -1;
        if (level == 1) {
            clock_gettime(CLOCK_MONOTONIC, &t_pulse_start);
            break;
        }
        clock_gettime(CLOCK_MONOTONIC, &t_now);
        if (elapsed_us(&t_wait_start, &t_now) > ECHO_WAIT_TIMEOUT_US) {
            *distance_cm = ULTRASONIC_NO_ECHO;
            return 0;
        }
    }

    /* wait for echo to go low */
    for (;;) {
        level = gpio_read(efd);
        if (level < 0) return -1;
        if (level == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t_pulse_end);
            break;
        }
        clock_gettime(CLOCK_MONOTONIC, &t_now);
        if (elapsed_us(&t_pulse_start, &t_now) > ECHO_WAIT_TIMEOUT_US) {
            *distance_cm = ULTRASONIC_NO_ECHO;
            return 0;
        }
    }

    long pulse_us = elapsed_us(&t_pulse_start, &t_pulse_end);
    *distance_cm = (pulse_us * SPEED_OF_SOUND_CM_PER_US) / 2.0f;
    return 0;
}

int ultrasonic_read_all(float distances[ULTRASONIC_NUM_SENSORS])
{
    int rc = 0;
    for (int i = 0; i < ULTRASONIC_NUM_SENSORS; i++) {
        if (ultrasonic_read_cm(i + 1, &distances[i]) < 0) {
            distances[i] = ULTRASONIC_NO_ECHO;
            rc = -1;
        }
        usleep(20000); /* let echoes settle before the next sensor fires */
    }
    return rc;
}
