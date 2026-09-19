/*
 * motor_test.c - Interactive QNX motor test for the MDD3A driver.
 *
 * IMPORTANT: Keep the wheels off the ground or the chassis secured for
 * the first run. Polarity of MOTOR1_FORWARD_* / MOTOR2_FORWARD_* in
 * motor.c is a PLACEHOLDER - do not trust "forward"/"backward" labels
 * until you've used raw mode below to confirm which pin combination
 * actually spins each wheel which way.
 *
 * Build (on Fedora, after sourcing qnxsdp-env.sh):
 *   qcc -Vgcc_ntoaarch64le -o motor_test tests/motor_test.c gpio.c motor.c -I.
 *
 * Deploy:
 *   scp motor_test qnxuser@192.168.50.98:/home/qnxuser/
 *
 * Run on the Pi:
 *   ./motor_test
 *
 * Menu:
 *   1 = forward
 *   2 = backward
 *   3 = left
 *   4 = right
 *   5 = stop
 *   r = raw mode: manually drive M1A M1B M2A M2B (e.g. "r 1 0 1 0")
 *       to discover actual polarity by watching the wheels
 *   s = set speed 0-100 (e.g. "s 40"), takes effect immediately
 *   t L R = raw UNTRIMMED per-wheel speed (e.g. "t 40 34") - use this to
 *       find the trim: run forward at "t 40 40" first, watch it veer,
 *       then nudge the slower wheel's number up (or the faster one's
 *       down) and re-run forward until it tracks straight. Whatever
 *       difference you land on is MOTOR_LEFT_TRIM_PCT/MOTOR_RIGHT_TRIM_PCT
 *       in motor.c - hardcode it there and rebuild so parking_app gets
 *       the same correction automatically via motor_set_speed().
 *   q = quit (stops motors first)
 */

#include <stdio.h>
#include <string.h>

#include "../gpio.h"
#include "../motor.h"

static void print_menu(void)
{
    printf("\n--- Motor Test ---\n");
    printf("1 = forward\n");
    printf("2 = backward\n");
    printf("3 = left\n");
    printf("4 = right\n");
    printf("5 = stop\n");
    printf("r a b c d = raw: M1A=a M1B=b M2A=c M2B=d (each 0 or 1)\n");
    printf("s N = set speed 0-100 percent (e.g. s 40)\n");
    printf("t L R = raw untrimmed per-wheel speed for calibration (e.g. t 40 34)\n");
    printf("q = quit\n");
    printf("> ");
    fflush(stdout);
}

int main(void)
{
    char line[64];

    if (motor_init() < 0) {
        fprintf(stderr, "motor_init failed - check GPIO permissions/wiring\n");
        return 1;
    }
    printf("Motors initialized and stopped.\n");

    for (;;) {
        print_menu();
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        if (line[0] == 'q') {
            break;
        } else if (line[0] == '1') {
            printf("-> forward\n");
            motor_forward();
        } else if (line[0] == '2') {
            printf("-> backward\n");
            motor_backward();
        } else if (line[0] == '3') {
            printf("-> left\n");
            motor_left();
        } else if (line[0] == '4') {
            printf("-> right\n");
            motor_right();
        } else if (line[0] == '5') {
            printf("-> stop\n");
            motor_stop();
        } else if (line[0] == 'r') {
            int a, b, c, d;
            if (sscanf(line + 1, "%d %d %d %d", &a, &b, &c, &d) == 4) {
                printf("-> raw M1A=%d M1B=%d M2A=%d M2B=%d\n", a, b, c, d);
                motor_set_raw(1, a, b);
                motor_set_raw(2, c, d);
            } else {
                printf("usage: r <M1A> <M1B> <M2A> <M2B>, e.g. r 1 0 0 0\n");
            }
        } else if (line[0] == 's') {
            int pct;
            if (sscanf(line + 1, "%d", &pct) == 1) {
                printf("-> speed %d%%\n", pct);
                motor_set_speed(pct);
            } else {
                printf("usage: s <0-100>, e.g. s 40\n");
            }
        } else if (line[0] == 't') {
            int left_pct, right_pct;
            if (sscanf(line + 1, "%d %d", &left_pct, &right_pct) == 2) {
                printf("-> raw speed left=%d%% right=%d%% (untrimmed)\n", left_pct, right_pct);
                motor_set_speed_lr(left_pct, right_pct);
            } else {
                printf("usage: t <left 0-100> <right 0-100>, e.g. t 40 34\n");
            }
        } else {
            printf("unrecognized input\n");
        }
    }

    printf("Stopping motors and exiting.\n");
    motor_deinit();
    return 0;
}
