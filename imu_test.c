/*
 * imu_test.c - Probes both /dev/i2c0 and /dev/i2c1 to find which bus the
 * MPU6050 is actually on (physical bus assignment is not assumed), then
 * streams accel/gyro/temp readings.
 *
 * Build:
 *   qcc -Vgcc_ntoaarch64le -o imu_test tests/imu_test.c i2c.c mpu6050.c -I.
 *
 * Deploy:
 *   scp imu_test qnxuser@192.168.50.98:/home/qnxuser/
 *
 * Run:
 *   ./imu_test
 *
 * NOTE ON THE I2C DEVCTL PROTOCOL:
 * i2c.c implements the standard QNX <hw/i2c.h> client devctl protocol
 * (DCMD_I2C_SENDRECV etc.) based on the header found in the SDK. This is
 * the documented client interface, but the exact buffer-layout
 * convention (send bytes and recv bytes sharing one buffer region) is
 * inferred from common QNX driver usage rather than a working example
 * we've run yet. If mpu6050_init() reports WHO_AM_I mismatches on BOTH
 * buses (rather than a clean "no response"), that's a signal the devctl
 * buffer layout needs adjusting - report the exact output back.
 */

#include <stdio.h>
#include <unistd.h>

#include "../mpu6050.h"

int main(void)
{
    int fd = -1;
    int bus_used = -1;

    for (int bus = 0; bus <= 1 && fd < 0; bus++) {
        printf("Probing /dev/i2c%d for MPU6050...\n", bus);
        fd = mpu6050_init(bus);
        if (fd >= 0) {
            bus_used = bus;
        }
    }

    if (fd < 0) {
        fprintf(stderr, "imu_test: MPU6050 not found on i2c0 or i2c1.\n");
        fprintf(stderr, "Check wiring (SDA->GPIO2 pin3, SCL->GPIO3 pin5, VCC->3.3V pin1, GND->pin6)\n");
        return 1;
    }

    printf("Using /dev/i2c%d. Streaming data (Ctrl+C to stop)...\n", bus_used);

    mpu6050_data_t data;
    for (;;) {
        if (mpu6050_read(fd, &data) < 0) {
            fprintf(stderr, "imu_test: read failed\n");
            break;
        }

        printf("accel(g): x=%6.3f y=%6.3f z=%6.3f  gyro(dps): x=%7.2f y=%7.2f z=%7.2f  temp=%.1fC\n",
               data.accel_x_g, data.accel_y_g, data.accel_z_g,
               data.gyro_x_dps, data.gyro_y_dps, data.gyro_z_dps,
               data.temp_c);

        usleep(200000); /* 5 Hz */
    }

    mpu6050_deinit(fd);
    return 0;
}
