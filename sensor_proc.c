/*
 * Sensor sampling process - reads ultrasonic + IMU, sends to decision_proc.
 * Owns its own hardware init since there's no shared memory across
 * processes to hand a fd through. Re-resolves IPC_NAME_DECISION if a
 * MsgSend outright fails, instead of hammering a dead connection.
 *
 * Build: qcc -Vgcc_ntoaarch64le -I. -o sensor_proc sensor_proc.c ipc.c \
 *            logger.c rt.c gpio.c ultrasonic.c i2c.c mpu6050.c
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <sys/neutrino.h>

#include "ipc.h"
#include "ultrasonic.h"
#include "mpu6050.h"
#include "rt.h"
#include "logger.h"

#define SENSOR_PROC_CPU_MASK 0x4u
#define SENSOR_PROC_PRIORITY_OFFSET 10

#define SENSOR_PERIOD_MS 250  /* target, not guaranteed - overruns just skip the sleep */

#define LATENCY_REPORT_EVERY 20

int main(void)
{
    plog_init("sensor_proc");

    int max_pri = sched_get_priority_max(SCHED_FIFO);
    rt_configure_self("sensor_proc", max_pri - SENSOR_PROC_PRIORITY_OFFSET, SENSOR_PROC_CPU_MASK);

    if (ultrasonic_init() < 0) {
        plog_critical("ultrasonic_init failed - check GPIO wiring/permissions");
        return 1;
    }

    int imu_fd = -1;
    for (int bus = 0; bus <= 1 && imu_fd < 0; bus++) {
        plog_info("probing /dev/i2c%d for MPU6050...", bus);
        imu_fd = mpu6050_init(bus);
    }
    if (imu_fd < 0) {
        plog_critical("MPU6050 not found on i2c0 or i2c1 - check wiring");
        return 1;
    }

    int decision_coid = ipc_client_connect(IPC_NAME_DECISION);
    plog_info("connected to decision_proc (coid=%d)", decision_coid);

    struct timespec next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick);

    long lat_min_us = -1, lat_max_us = -1;
    long lat_sum_us = 0;
    int lat_count = 0;

    for (;;) {
        sensor_update_msg_t msg;
        msg.type = MSG_SENSOR_UPDATE;

        ultrasonic_read_all(msg.us_cm);

        if (mpu6050_read(imu_fd, &msg.imu) < 0) {
            plog_warn("mpu6050_read failed");
        }

        struct timespec t_before, t_after;
        clock_gettime(CLOCK_MONOTONIC, &t_before);

        ack_msg_t ack = { 0 };
        if (MsgSend(decision_coid, &msg, sizeof(msg), &ack, sizeof(ack)) == -1) {
            plog_error("MsgSend to decision_proc failed: %s - re-resolving \"%s\"",
                       strerror(errno), IPC_NAME_DECISION);
            decision_coid = ipc_client_connect(IPC_NAME_DECISION);
        } else if (ack.shutdown) {
            plog_info("shutdown received, stopping sampling and exiting");
            ultrasonic_deinit();
            mpu6050_deinit(imu_fd);
            return 0;
        } else {
            clock_gettime(CLOCK_MONOTONIC, &t_after);
            long us = (t_after.tv_sec - t_before.tv_sec) * 1000000L +
                      (t_after.tv_nsec - t_before.tv_nsec) / 1000L;

            if (lat_min_us < 0 || us < lat_min_us) lat_min_us = us;
            if (us > lat_max_us) lat_max_us = us;
            lat_sum_us += us;
            lat_count++;

            if (lat_count >= LATENCY_REPORT_EVERY) {
                plog_info("IPC round-trip latency over last %d msgs: min=%ldus max=%ldus avg=%ldus",
                          lat_count, lat_min_us, lat_max_us, lat_sum_us / lat_count);
                lat_min_us = -1;
                lat_max_us = -1;
                lat_sum_us = 0;
                lat_count = 0;
            }
        }

        next_tick.tv_nsec += (long)SENSOR_PERIOD_MS * 1000000L;
        while (next_tick.tv_nsec >= 1000000000L) {
            next_tick.tv_nsec -= 1000000000L;
            next_tick.tv_sec += 1;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_tick, NULL);
    }

    return 0;
}
