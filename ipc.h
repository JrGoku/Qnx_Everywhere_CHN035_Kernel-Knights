/*
 * Message formats and process discovery for sensor_proc/decision_proc/
 * motor_proc. Since they're separate processes (no shared chid global
 * like a shared-thread design would have), servers register a
 * name in QNX's global name space (name_attach(), <sys/dispatch.h>) and
 * clients look it up (name_open()) - see ipc_server_attach()/
 * ipc_client_connect() in ipc.c. Startup order doesn't matter this way.
 */

#ifndef IPC_H
#define IPC_H

#include "ultrasonic.h"
#include "mpu6050.h"

typedef enum {
    MSG_SENSOR_UPDATE = 1,
    MSG_MOTOR_COMMAND = 2,
    MSG_SHUTDOWN = 3,  /* sent once, on reaching PARK_STATE_PARKED */
} msg_type_t;

/* sensor_proc -> decision_proc */
typedef struct {
    msg_type_t type;
    float us_cm[ULTRASONIC_NUM_SENSORS];
    mpu6050_data_t imu;
} sensor_update_msg_t;

/* decision_proc -> motor_proc
 * left_dir/right_dir: -1 = backward, 0 = stop, 1 = forward (per wheel,
 * motor1=left/motor2=right per the confirmed motor.c polarity table).
 * speed_pct: 0-100, applied as the PWM duty for both wheels. */
typedef struct {
    msg_type_t type;
    int left_dir;
    int right_dir;
    int speed_pct;
} motor_command_msg_t;

typedef struct {
    int ok;
    int shutdown;  /* set once by decision_proc, on reaching PARKED */
} ack_msg_t;

#define IPC_NAME_DECISION "parking/decision"
#define IPC_NAME_MOTOR    "parking/motor"

int ipc_server_attach(const char *name);   /* name_attach(), exits on failure */
int ipc_client_connect(const char *name);  /* name_open(), retries until found */

#endif /* IPC_H */
