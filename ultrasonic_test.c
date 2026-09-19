/*
 * ultrasonic_test.c - Prints live distance readings from all 8
 * HC-SR04-type sensors, once per second, sequentially triggered.
 *
 * Build:
 *   qcc -Vgcc_ntoaarch64le -o ultrasonic_test tests/ultrasonic_test.c gpio.c ultrasonic.c -I.
 *
 * Deploy:
 *   scp ultrasonic_test qnxuser@192.168.50.98:/home/qnxuser/
 *
 * Run:
 *   ./ultrasonic_test
 *   (Ctrl+C to stop)
 *
 * Output format:
 *   US1 = 23.4 cm   US2 =  --  no echo   US3 = 41.0 cm  ...
 *
 * NOTE: Which physical direction (front/left/right/etc.) each USn
 * corresponds to is NOT yet determined - that's a separate manual
 * mapping step. This test just confirms the raw driver works and lets
 * you identify USn <-> physical position by covering one sensor at a
 * time and watching which line in the output changes.
 */

#include <stdio.h>
#include <unistd.h>

#include "../ultrasonic.h"

int main(void)
{
    if (ultrasonic_init() < 0) {
        fprintf(stderr, "ultrasonic_init failed - check GPIO wiring/permissions\n");
        return 1;
    }
    printf("Ultrasonic sensors initialized. Reading... (Ctrl+C to stop)\n");

    float distances[ULTRASONIC_NUM_SENSORS];

    for (;;) {
        ultrasonic_read_all(distances);

        for (int i = 0; i < ULTRASONIC_NUM_SENSORS; i++) {
            if (distances[i] == ULTRASONIC_NO_ECHO) {
                printf("US%d = --no echo--  ", i + 1);
            } else {
                printf("US%d = %6.1f cm  ", i + 1, distances[i]);
            }
        }
        printf("\n");
        fflush(stdout);

        sleep(1);
    }

    ultrasonic_deinit();
    return 0;
}
