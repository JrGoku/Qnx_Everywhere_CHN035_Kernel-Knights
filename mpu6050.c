#include <stdio.h>

#include "i2c.h"
#include "mpu6050.h"

#define REG_PWR_MGMT_1   0x6B
#define REG_ACCEL_XOUT_H 0x3B  /* 14 contiguous bytes: accel(6) temp(2) gyro(6) */

/* Default sensitivity at power-on (no config write done): +-2g, +-250 dps. */
#define ACCEL_SCALE (16384.0f)  /* LSB per g */
#define GYRO_SCALE  (131.0f)    /* LSB per deg/s */

static int16_t be16(uint8_t hi, uint8_t lo)
{
    return (int16_t)((hi << 8) | lo);
}

int mpu6050_init(int bus)
{
    int fd = i2c_open(bus);
    if (fd < 0) {
        return -1;
    }

    uint8_t who = 0;
    if (i2c_read_regs(fd, MPU6050_ADDR, MPU6050_REG_WHO_AM_I, &who, 1) < 0) {
        fprintf(stderr, "mpu6050_init: no response on i2c bus %d\n", bus);
        i2c_close(fd);
        return -1;
    }

    if (who != 0x68) {
        fprintf(stderr, "mpu6050_init: unexpected WHO_AM_I=0x%02x on bus %d (expected 0x68)\n",
                who, bus);
        i2c_close(fd);
        return -1;
    }

    /* Wake the device up: PWR_MGMT_1 defaults to sleep=1 on power-on. */
    if (i2c_write_reg(fd, MPU6050_ADDR, REG_PWR_MGMT_1, 0x00) < 0) {
        fprintf(stderr, "mpu6050_init: failed to wake device on bus %d\n", bus);
        i2c_close(fd);
        return -1;
    }

    printf("mpu6050_init: found MPU6050 on /dev/i2c%d (WHO_AM_I=0x%02x)\n", bus, who);
    return fd;
}

void mpu6050_deinit(int fd)
{
    i2c_close(fd);
}

int mpu6050_read(int fd, mpu6050_data_t *data)
{
    uint8_t raw[14];

    if (i2c_read_regs(fd, MPU6050_ADDR, REG_ACCEL_XOUT_H, raw, 14) < 0) {
        return -1;
    }

    int16_t ax = be16(raw[0], raw[1]);
    int16_t ay = be16(raw[2], raw[3]);
    int16_t az = be16(raw[4], raw[5]);
    int16_t temp_raw = be16(raw[6], raw[7]);
    int16_t gx = be16(raw[8], raw[9]);
    int16_t gy = be16(raw[10], raw[11]);
    int16_t gz = be16(raw[12], raw[13]);

    data->accel_x_g = ax / ACCEL_SCALE;
    data->accel_y_g = ay / ACCEL_SCALE;
    data->accel_z_g = az / ACCEL_SCALE;

    data->gyro_x_dps = gx / GYRO_SCALE;
    data->gyro_y_dps = gy / GYRO_SCALE;
    data->gyro_z_dps = gz / GYRO_SCALE;

    /* Per MPU6050 datasheet: Temp_degC = raw/340 + 36.53 */
    data->temp_c = (temp_raw / 340.0f) + 36.53f;

    return 0;
}
