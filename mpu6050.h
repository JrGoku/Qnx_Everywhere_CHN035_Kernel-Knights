/*
 * MPU6050 IMU driver over I2C.
 * Wiring: VCC->3.3V, GND->GND, SDA->GPIO2, SCL->GPIO3. Address 0x68
 * (would be 0x69 if AD0 were tied high, not the case here).
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPU6050_ADDR        0x68
#define MPU6050_REG_WHO_AM_I 0x75  /* should read back 0x68 */

typedef struct {
    float accel_x_g, accel_y_g, accel_z_g;   /* in units of g */
    float gyro_x_dps, gyro_y_dps, gyro_z_dps; /* in degrees/sec */
    float temp_c;
} mpu6050_data_t;

int mpu6050_init(int bus);   /* bus 0 or 1, returns fd or -1 */
void mpu6050_deinit(int fd);
int mpu6050_read(int fd, mpu6050_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
