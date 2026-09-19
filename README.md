# Autonomous Parking Robot - QNX 8.0 / Raspberry Pi 4

Active architecture: **three independent QNX processes**, communicating by
native QNX message passing and discovering each other through the QNX
kernel's global name space - not shared threads/memory. See each file's
header comment for the reasoning:

```
sensor_proc --MsgSend(SensorUpdate)--> decision_proc --MsgSend(MotorCommand)--> motor_proc
```

- `sensor_proc.c` - samples all 8 ultrasonics + the MPU6050 on a timer, sends to decision_proc.
- `decision_proc.c` - runs the parking state machine (`parking.c`/`parking.h`) and forwards motor commands.
- `motor_proc.c` - applies motor commands to hardware; has its own watchdog so it fails safe even if decision_proc dies.
- `ipc.h`/`ipc.c` - message formats + `name_attach()`/`name_open()`-based process discovery.
- `logger.h`/`logger.c` - `plog_info/warn/error/critical()`, wrapping QNX's real system logger (`slogf`, read back later with `sloginfo`) and mirroring to stdout/stderr.
- `rt.c`/`rt.h` - SCHED_FIFO priority + CPU-affinity setup, one call per process.
- `gpio.*`, `motor.*`, `ultrasonic.*`, `i2c.*`, `mpu6050.*` - hardware drivers, shared by all three processes.
- `motor_test`, `ultrasonic_test`, `imu_test` - standalone hardware test binaries.
- `run_parking.sh` / `stop_parking.sh` - start/stop all three processes together (nohup-based, survives an SSH/Ethernet disconnect).

## Build & deploy

```
source ~/qnx800/qnxsdp-env.sh
make                                   # builds tests + sensor_proc/decision_proc/motor_proc
make deploy PI_HOST=qnxuser@<pi-ip>
```

## Run on the Pi

```
./run_parking.sh     # starts all three, detached, logs to *_proc.log
./stop_parking.sh    # stops all three
```

The processes shut themselves down automatically once parking succeeds
(SEARCHING -> ALIGNING -> PARKING -> PARKED) - no manual stop needed in
the normal case.
