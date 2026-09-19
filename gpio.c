#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "gpio.h"

int gpio_open(int pin)
{
    char path[32];
    snprintf(path, sizeof(path), "/dev/gpio/%d", pin);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "gpio_open: failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }
    return fd;
}

void gpio_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

int gpio_set_output(int fd)
{
    if (write(fd, "out", 3) != 3) {
        fprintf(stderr, "gpio_set_output: write failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int gpio_set_input(int fd)
{
    if (write(fd, "in", 2) != 2) {
        fprintf(stderr, "gpio_set_input: write failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int gpio_write(int fd, int value)
{
    int rc;
    if (value) {
        rc = write(fd, "on", 2);
        if (rc != 2) {
            fprintf(stderr, "gpio_write(on): write failed: %s\n", strerror(errno));
            return -1;
        }
    } else {
        rc = write(fd, "off", 3);
        if (rc != 3) {
            fprintf(stderr, "gpio_write(off): write failed: %s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

int gpio_read(int fd)
{
    char buf[8];
    ssize_t n;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        fprintf(stderr, "gpio_read: lseek failed: %s\n", strerror(errno));
        return -1;
    }

    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        fprintf(stderr, "gpio_read: read failed: %s\n", strerror(errno));
        return -1;
    }
    if (n == 0) {
        fprintf(stderr, "gpio_read: empty read\n");
        return -1;
    }
    buf[n] = '\0';

    /* Value is ASCII '0' or '1', possibly with trailing whitespace/newline. */
    if (buf[0] == '1') return 1;
    if (buf[0] == '0') return 0;

    fprintf(stderr, "gpio_read: unexpected value '%s'\n", buf);
    return -1;
}
