/*
 * Decision/state-machine process - runs parking.c between sensor_proc
 * and motor_proc. If sensor_proc stalls (no update within
 * SENSOR_WATCHDOG_NS), sends motor_proc a stop directly rather than
 * waiting on a dead peer.
 *
 * Build: qcc -Vgcc_ntoaarch64le -I. -o decision_proc decision_proc.c \
 *            ipc.c logger.c rt.c parking.c
 */

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <string.h>
#include <sys/neutrino.h>

#include "ipc.h"
#include "parking.h"
#include "rt.h"
#include "logger.h"

#define DECISION_PROC_CPU_MASK 0x2u
#define DECISION_PROC_PRIORITY_OFFSET 5

#define SENSOR_WATCHDOG_NS (1000ULL * 1000ULL * 1000ULL) /* 1 second */

#define LOOP_LATENCY_REPORT_EVERY 20

int main(void)
{
    plog_init("decision_proc");

    int max_pri = sched_get_priority_max(SCHED_FIFO);
    rt_configure_self("decision_proc", max_pri - DECISION_PROC_PRIORITY_OFFSET,
                       DECISION_PROC_CPU_MASK);

    parking_init();

    int chid = ipc_server_attach(IPC_NAME_DECISION);
    int motor_coid = ipc_client_connect(IPC_NAME_MOTOR);

    plog_info("ready (chid=%d), connected to motor_proc (coid=%d)", chid, motor_coid);

    parking_state_t last_state = (parking_state_t)-1;

    /* full-loop latency: sensor-recv to motor-ack, rolling + per-tick */
    long loop_lat_min_us = -1, loop_lat_max_us = -1;
    long loop_lat_sum_us = 0;
    int loop_lat_count = 0;

    for (;;) {
        uint64_t timeout_ns = SENSOR_WATCHDOG_NS;
        TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &timeout_ns, NULL);

        sensor_update_msg_t sensor_msg;
        int rcvid = MsgReceive(chid, &sensor_msg, sizeof(sensor_msg), NULL);
        struct timespec t_recv;
        clock_gettime(CLOCK_MONOTONIC, &t_recv);

        if (rcvid == -1) {
            if (errno == ETIMEDOUT) {
                plog_error("WATCHDOG - no sensor update from sensor_proc in %llus, "
                           "forcing motor stop as a fail-safe",
                           (unsigned long long)(SENSOR_WATCHDOG_NS / 1000000000ULL));
                motor_command_msg_t stop_cmd = { .type = MSG_MOTOR_COMMAND,
                                                  .left_dir = 0, .right_dir = 0, .speed_pct = 0 };
                ack_msg_t motor_ack;
                MsgSend(motor_coid, &stop_cmd, sizeof(stop_cmd), &motor_ack, sizeof(motor_ack));
            } else {
                plog_error("MsgReceive failed: %s", strerror(errno));
            }
            continue;
        }
        if (rcvid == 0) {
            continue; /* pulse, unused */
        }

        if (sensor_msg.type != MSG_SENSOR_UPDATE) {
            ack_msg_t ack = { .ok = 1, .shutdown = 0 };
            MsgReply(rcvid, EOK, &ack, sizeof(ack));
            continue;
        }

        motor_command_msg_t cmd;
        cmd.type = MSG_MOTOR_COMMAND;

        parking_state_t state = parking_update(&sensor_msg, &cmd);
        if (state != last_state) {
            plog_info("state -> %s", parking_state_name(state));
            last_state = state;
        }
        int parked = (state == PARK_STATE_PARKED);

        ack_msg_t ack = { .ok = 1, .shutdown = parked };
        MsgReply(rcvid, EOK, &ack, sizeof(ack));

        if (parked) {
            cmd.type = MSG_SHUTDOWN;
        }

        ack_msg_t motor_ack;
        if (MsgSend(motor_coid, &cmd, sizeof(cmd), &motor_ack, sizeof(motor_ack)) == -1) {
            plog_error("MsgSend to motor_proc failed: %s", strerror(errno));
        } else {
            struct timespec t_motor_done;
            clock_gettime(CLOCK_MONOTONIC, &t_motor_done);
            long loop_us = (t_motor_done.tv_sec - t_recv.tv_sec) * 1000000L +
                           (t_motor_done.tv_nsec - t_recv.tv_nsec) / 1000L;

            if (state == PARK_STATE_EMERGENCY_STOP) {
                plog_info("EMERGENCY_STOP latency: sensor-recv-to-motor-ack = %ld us", loop_us);
            }

            if (loop_lat_min_us < 0 || loop_us < loop_lat_min_us) loop_lat_min_us = loop_us;
            if (loop_us > loop_lat_max_us) loop_lat_max_us = loop_us;
            loop_lat_sum_us += loop_us;
            loop_lat_count++;

            if (loop_lat_count >= LOOP_LATENCY_REPORT_EVERY) {
                plog_info("full-loop (sensor-recv to motor-ack) latency over last %d ticks: "
                          "min=%ldus max=%ldus avg=%ldus",
                          loop_lat_count, loop_lat_min_us, loop_lat_max_us,
                          loop_lat_sum_us / loop_lat_count);
                loop_lat_min_us = -1;
                loop_lat_max_us = -1;
                loop_lat_sum_us = 0;
                loop_lat_count = 0;
            }
        }

        if (parked) {
            plog_info("parked - shutting down");
            break;
        }
    }

    return 0;
}
