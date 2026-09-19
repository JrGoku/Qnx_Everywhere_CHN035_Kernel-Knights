/*
 * Wrapper around rpi_gpio's per-pin files at /dev/gpio/<N>.
 * Reference (QNX 8.0, RPi4 BSP, `use rpi_gpio`):
 *   write(fd,"out",3)/"in",2  -> set direction
 *   write(fd,"on",2)/"off",3  -> drive an output pin
 *   read(fd,buf,n)            -> "0"/"1" for current level
 *
 * Open each pin once and keep the fd - re-opening per access is slow,
 * and the ultrasonic echo timing needs fast repeated reads anyway.
 */

#ifndef GPIO_H
#define GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

int gpio_open(int pin);        /* fd >= 0, or -1 on error */
void gpio_close(int fd);
int gpio_set_output(int fd);
int gpio_set_input(int fd);
int gpio_write(int fd, int value);  /* value = 0 or 1 */
int gpio_read(int fd);              /* returns 0, 1, or -1 on error */

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */
