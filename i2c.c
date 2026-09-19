#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <devctl.h>

#include <hw/i2c.h>

#include "i2c.h"

int i2c_open(int bus)
{
    char path[16];
    snprintf(path, sizeof(path), "/dev/i2c%d", bus);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "i2c_open: failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }
    return fd;
}

void i2c_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

int i2c_write_reg(int fd, uint8_t addr, uint8_t reg, uint8_t val)
{
    struct {
        i2c_send_t hdr;
        uint8_t buf[2];
    } send_buf;

    send_buf.hdr.slave.addr = addr;
    send_buf.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
    send_buf.hdr.len = 2;
    send_buf.hdr.stop = 1;
    send_buf.buf[0] = reg;
    send_buf.buf[1] = val;

    int rc = devctl(fd, DCMD_I2C_SEND, &send_buf, sizeof(send_buf), NULL);
    if (rc != EOK) {
        fprintf(stderr, "i2c_write_reg: devctl DCMD_I2C_SEND failed: %s\n", strerror(rc));
        return -1;
    }
    return 0;
}

int i2c_read_regs(int fd, uint8_t addr, uint8_t reg, uint8_t *buf, int len)
{
    if (len <= 0 || len > 64) {
        fprintf(stderr, "i2c_read_regs: invalid len %d\n", len);
        return -1;
    }

    /* buf holds both the outgoing reg byte and the incoming data */
    struct {
        i2c_sendrecv_t hdr;
        uint8_t buf[64];
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.hdr.slave.addr = addr;
    msg.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
    msg.hdr.send_len = 1;
    msg.hdr.recv_len = (uint32_t)len;
    msg.hdr.stop = 1;
    msg.buf[0] = reg;

    int dcmd_len = (int)sizeof(msg.hdr) + (len > 1 ? len : 1);

    int rc = devctl(fd, DCMD_I2C_SENDRECV, &msg, dcmd_len, NULL);
    if (rc != EOK) {
        fprintf(stderr, "i2c_read_regs: devctl DCMD_I2C_SENDRECV failed: %s\n", strerror(rc));
        return -1;
    }

    memcpy(buf, msg.buf, len);
    return 0;
}

int i2c_probe(int fd, uint8_t addr, uint8_t reg)
{
    uint8_t val;
    return i2c_read_regs(fd, addr, reg, &val, 1);
}
