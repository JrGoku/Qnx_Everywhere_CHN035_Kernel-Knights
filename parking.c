#include <math.h>
#include <string.h>
#include <time.h>

#include "parking.h"
#include "logger.h"

/* sensor indices: us1=front, us3=right, us5=rear */
#define SENSOR_FRONT      0
#define SENSOR_SIDE_SCAN  2
#define SENSOR_REAR       4

#define EMERGENCY_STOP_CM 10.0f

/* box reads ~6-8cm, open slot ~41-44cm on this chassis - big enough gap
 * that a flat threshold beats trying to detect a relative jump */
#define OPEN_THRESHOLD_CM    20.0f
#define NO_ECHO_FALLBACK_CM  200.0f
#define OPEN_CONFIRM_MS      500L

#define ALIGN_FORWARD_MS       800L
#define TARGET_TURN_DEG        85.0f
#define PARK_WALL_CM            20.0f
#define PARK_INTO_SPOT_FORWARD 0      /* 1 = forward into spot, 0 = reverse */

#define SEARCH_SPEED_PCT  30
#define TURN_SPEED_PCT    35
#define PARK_SPEED_PCT    30

typedef struct {
    parking_state_t state, pre_emergency_state;

    float side_filtered_cm;
    long  opening_ms, align_ms;

    int   turning;            /* ALIGNING sub-phase: 0 = centering, 1 = pivoting */
    float heading_accum_deg;

    struct timespec last_tick;
    int have_last_tick;
} parking_ctx_t;

static parking_ctx_t ctx;

static long ms_since(const struct timespec *a, const struct timespec *b)
{
    return (b->tv_sec - a->tv_sec) * 1000L + (b->tv_nsec - a->tv_nsec) / 1000000L;
}

static void stop_cmd(motor_command_msg_t *cmd)
{
    cmd->left_dir = cmd->right_dir = cmd->speed_pct = 0;
}

void parking_init(void)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = PARK_STATE_SEARCHING;
}

const char *parking_state_name(parking_state_t state)
{
    switch (state) {
        case PARK_STATE_SEARCHING:      return "SEARCHING";
        case PARK_STATE_ALIGNING:       return "ALIGNING";
        case PARK_STATE_PARKING:        return "PARKING";
        case PARK_STATE_PARKED:         return "PARKED";
        case PARK_STATE_EMERGENCY_STOP: return "EMERGENCY_STOP";
        default:                        return "UNKNOWN";
    }
}

parking_state_t parking_update(const sensor_update_msg_t *s, motor_command_msg_t *cmd)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long dt_ms = ctx.have_last_tick ? ms_since(&ctx.last_tick, &now) : 1;
    if (dt_ms <= 0) dt_ms = 1;
    ctx.last_tick = now;
    ctx.have_last_tick = 1;

    float front_cm = s->us_cm[SENSOR_FRONT];
    float rear_cm  = s->us_cm[SENSOR_REAR];
    float side_cm  = s->us_cm[SENSOR_SIDE_SCAN];

    /* overrides every state except PARKED until the obstacle clears */
    int obstacle_close = (front_cm > 0.0f && front_cm < EMERGENCY_STOP_CM) ||
                          (rear_cm  > 0.0f && rear_cm  < EMERGENCY_STOP_CM);
    if (obstacle_close && ctx.state != PARK_STATE_EMERGENCY_STOP && ctx.state != PARK_STATE_PARKED) {
        ctx.pre_emergency_state = ctx.state;
        ctx.state = PARK_STATE_EMERGENCY_STOP;
    }

    switch (ctx.state) {

    case PARK_STATE_EMERGENCY_STOP:
        stop_cmd(cmd);
        if (!obstacle_close) {
            ctx.state = ctx.pre_emergency_state;
        }
        break;

    case PARK_STATE_SEARCHING: {
        cmd->left_dir = cmd->right_dir = 1;
        cmd->speed_pct = SEARCH_SPEED_PCT;

        /* no echo = out of range, treat as far rather than 0 */
        float sample = (side_cm <= 0.0f) ? NO_ECHO_FALLBACK_CM : side_cm;
        ctx.side_filtered_cm = (ctx.side_filtered_cm <= 0.0f)
                                  ? sample
                                  : 0.7f * ctx.side_filtered_cm + 0.3f * sample;

        if (ctx.side_filtered_cm >= OPEN_THRESHOLD_CM) {
            ctx.opening_ms += dt_ms;
        } else {
            ctx.opening_ms = 0;
        }

        if (ctx.opening_ms >= OPEN_CONFIRM_MS) {
            ctx.state = PARK_STATE_ALIGNING;
            ctx.align_ms = 0;
            ctx.turning = 0;
            ctx.heading_accum_deg = 0.0f;
        }
        break;
    }

    case PARK_STATE_ALIGNING:
        if (!ctx.turning) {
            cmd->left_dir = cmd->right_dir = 1;
            cmd->speed_pct = SEARCH_SPEED_PCT;

            ctx.align_ms += dt_ms;
            if (ctx.align_ms >= ALIGN_FORWARD_MS) {
                stop_cmd(cmd);
                ctx.turning = 1;
                ctx.heading_accum_deg = 0.0f;
            }
        } else {
            ctx.heading_accum_deg += s->imu.gyro_z_dps * (dt_ms / 1000.0f);
            cmd->left_dir  = -1;
            cmd->right_dir = 1;
            cmd->speed_pct = TURN_SPEED_PCT;

            /* gyro reads negative dps for this turn direction, so check
             * magnitude, not sign - see debug log from the turn-that-
             * never-stopped bug */
            if (fabsf(ctx.heading_accum_deg) >= TARGET_TURN_DEG) {
                stop_cmd(cmd);
                ctx.state = PARK_STATE_PARKING;
            }
        }
        break;

    case PARK_STATE_PARKING: {
        float wall_cm = PARK_INTO_SPOT_FORWARD ? front_cm : rear_cm;

        if (wall_cm > 0.0f && wall_cm <= PARK_WALL_CM) {
            stop_cmd(cmd);
            ctx.state = PARK_STATE_PARKED;
            break;
        }

        cmd->left_dir  = PARK_INTO_SPOT_FORWARD ? 1 : -1;
        cmd->right_dir = cmd->left_dir;
        cmd->speed_pct = PARK_SPEED_PCT;
        break;
    }

    case PARK_STATE_PARKED:
    default:
        stop_cmd(cmd);
        break;
    }

    return ctx.state;
}
