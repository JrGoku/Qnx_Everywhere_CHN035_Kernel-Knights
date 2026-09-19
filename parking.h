/*
 * Parking state machine: SEARCHING -> ALIGNING (center + pivot) ->
 * PARKING (drive in until the wall is close) -> PARKED. EMERGENCY_STOP
 * overrides any state on a close front/rear obstacle, resuming after.
 * Called once per sensor update; timers use clock_gettime so behavior
 * doesn't depend on the sensor loop's exact period.
 */

#ifndef PARKING_H
#define PARKING_H

#include "ipc.h"

typedef enum {
    PARK_STATE_SEARCHING = 0,
    PARK_STATE_ALIGNING,
    PARK_STATE_PARKING,
    PARK_STATE_PARKED,
    PARK_STATE_EMERGENCY_STOP
} parking_state_t;

void parking_init(void);
parking_state_t parking_update(const sensor_update_msg_t *sensors, motor_command_msg_t *cmd_out);
const char *parking_state_name(parking_state_t state);

#endif /* PARKING_H */
