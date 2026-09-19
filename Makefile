# Makefile for the multi-process QNX parking system (cross-compiled on
# Fedora for QNX 8.0 / Raspberry Pi 4, aarch64).
#
# Usage:
#   source ~/qnx800/qnxsdp-env.sh
#   make            # builds the hardware test binaries + the 3 processes
#   make deploy     # scp's them to the Pi (override PI_HOST as needed, e.g.
#                   #   make deploy PI_HOST=qnxuser@172.17.156.0)
#   make clean
#

CC = qcc
TARGET = -Vgcc_ntoaarch64le
CFLAGS = -I.
PI_HOST = qnxuser@192.168.50.98

PROC_COMMON = ipc.c logger.c rt.c
PROC_COMMON_HDRS = ipc.h rt.h logger.h

all: motor_test ultrasonic_test imu_test sensor_proc decision_proc motor_proc

motor_test: tests/motor_test.c gpio.c motor.c gpio.h motor.h
	$(CC) $(TARGET) $(CFLAGS) -o motor_test tests/motor_test.c gpio.c motor.c

ultrasonic_test: tests/ultrasonic_test.c gpio.c ultrasonic.c gpio.h ultrasonic.h
	$(CC) $(TARGET) $(CFLAGS) -o ultrasonic_test tests/ultrasonic_test.c gpio.c ultrasonic.c

imu_test: tests/imu_test.c i2c.c mpu6050.c i2c.h mpu6050.h
	$(CC) $(TARGET) $(CFLAGS) -o imu_test tests/imu_test.c i2c.c mpu6050.c

# Three independent QNX processes, discovering each other by name (see
# ipc.h/ipc.c) instead of sharing memory - a crash in one can no longer
# corrupt or take down the others. All log through logger.c (slogf, see
# logger.h) in addition to stdout/stderr.

sensor_proc: sensor_proc.c $(PROC_COMMON) gpio.c ultrasonic.c i2c.c mpu6050.c \
             $(PROC_COMMON_HDRS) ultrasonic.h i2c.h mpu6050.h
	$(CC) $(TARGET) $(CFLAGS) -o sensor_proc sensor_proc.c ipc.c logger.c rt.c \
	    gpio.c ultrasonic.c i2c.c mpu6050.c

decision_proc: decision_proc.c $(PROC_COMMON) parking.c $(PROC_COMMON_HDRS) parking.h
	$(CC) $(TARGET) $(CFLAGS) -o decision_proc decision_proc.c ipc.c logger.c rt.c parking.c

motor_proc: motor_proc.c $(PROC_COMMON) gpio.c motor.c $(PROC_COMMON_HDRS) motor.h
	$(CC) $(TARGET) $(CFLAGS) -o motor_proc motor_proc.c ipc.c logger.c rt.c gpio.c motor.c

deploy: all
	scp motor_test ultrasonic_test imu_test \
	    sensor_proc decision_proc motor_proc run_parking.sh stop_parking.sh \
	    $(PI_HOST):/home/qnxuser/

clean:
	rm -f motor_test ultrasonic_test imu_test sensor_proc decision_proc motor_proc
