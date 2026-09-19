/*
 * Wrapper around QNX's I2C resource-manager client interface
 * (devctl() on /dev/i2cN, <hw/i2c.h>).
 *
 * Pi has two controllers, /dev/i2c0 and /dev/i2c1 - don't assume which
 * one the MPU6050 is wired to, probe both and see which ACKs.
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int i2c_open(int bus);    /* bus = 0 or 1 */
void i2c_close(int fd);
int i2c_write_reg(int fd, uint8_t addr, uint8_t reg, uint8_t val);
int i2c_read_regs(int fd, uint8_t addr, uint8_t reg, uint8_t *buf, int len);
int i2c_probe(int fd, uint8_t addr, uint8_t reg);  /* 0 if addr ACKs, else -1 */

#ifdef __cplusplus
}
#endif

#endif /* I2C_H */
