/*
 * Motor control process - highest SCHED_FIFO priority of the three,
 * own CPU core, since this is the one that can hit something. Self-
 * watchdogs decision_proc via TimerTimeout: no command in
 * DECISION_WATCHDOG_NS forces a stop here rather than trusting the
 * other side to have sent one before dying.
 *
 * Build: qcc -Vgcc_ntoaarch64le -I. -o motor_proc motor_proc.c ipc.c \
 *            logger.c rt.c gpio.c motor.c
 */

#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <string.h>
#include <sys/neutrino.h>

#include "ipc.h"
#include "motor.h"
#include "rt.h"
#include "logger.h"

#define MOTOR_PROC_CPU_MASK 0x1u

#define DECISION_WATCHDOG_NS (1000ULL * 1000ULL * 1000ULL) /* 1 second */

int main(void)
{
    plog_init("motor_proc");

    /* must run before rt_configure_self() - motor_init() spawns the PWM
     * thread with default pthread attrs, which inherits the CALLING
     * thread's priority on QNX. Elevate first and the PWM thread also
     * goes max SCHED_FIFO, and two threads at max priority once froze
     * the whole board (SSH included). motor_init() first keeps the PWM
     * thread at normal priority; only main (the MsgReceive/MsgReply
     * loop) gets the real-time bump. */
    if (motor_init() < 0) {
        plog_critical("motor_init failed - check GPIO wiring/permissions");
        return 1;
    }

    rt_configure_self("motor_proc", sched_get_priority_max(SCHED_FIFO), MOTOR_PROC_CPU_MASK);

    int chid = ipc_server_attach(IPC_NAME_MOTOR);

    plog_info("ready, waiting for commands as \"%s\" (chid=%d)", IPC_NAME_MOTOR, chid);

    for (;;) {
        uint64_t timeout_ns = DECISION_WATCHDOG_NS;
        TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &timeout_ns, NULL);

        motor_command_msg_t cmd;
        int rcvid = MsgReceive(chid, &cmd, sizeof(cmd), NULL);

        if (rcvid == -1) {
            if (errno == ETIMEDOUT) {
                plog_error("WATCHDOG - no command from decision_proc in %llus, "
                           "forcing motor stop as a fail-safe",
                           (unsigned long long)(DECISION_WATCHDOG_NS / 1000000000ULL));
                motor_stop();
            } else {
                plog_error("MsgReceive failed: %s", strerror(errno));
            }
            continue;
        }
        if (rcvid == 0) {
            continue; /* pulse, unused */
        }

        ack_msg_t ack = { .ok = 1, .shutdown = 0 };

        if (cmd.type == MSG_MOTOR_COMMAND) {
            motor_set_speed(cmd.speed_pct);
            motor_drive(cmd.left_dir, cmd.right_dir);
        } else if (cmd.type == MSG_SHUTDOWN) {
            motor_stop();
            ack.shutdown = 1;
        }

        MsgReply(rcvid, EOK, &ack, sizeof(ack));

        if (cmd.type == MSG_SHUTDOWN) {
            plog_info("shutdown received, motors stopped, exiting");
            break;
        }
    }

    motor_deinit();
    return 0;
}
