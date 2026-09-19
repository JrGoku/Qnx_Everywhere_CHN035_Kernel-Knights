#!/bin/sh
# stop_parking.sh - stops all three parking processes started by
# run_parking.sh.
#
# Killing them in any order is actually safe: motor_proc's own watchdog
# (see motor_proc.c) will force a motor_stop() within ~1s of decision_proc
# disappearing even if this kill signal reaches motor_proc first - that's
# the fault-isolation design working as intended, not a race to worry
# about.

PIDFILE=/tmp/parking_pids

if [ ! -f "$PIDFILE" ]; then
    echo "No $PIDFILE found - nothing to stop (already stopped, or never started with run_parking.sh)."
    exit 1
fi

PIDS=$(cat "$PIDFILE")
echo "Stopping: $PIDS"
kill $PIDS 2>/dev/null

sleep 1
rm -f "$PIDFILE"
echo "Done. (If a motor was moving, motor_proc's own watchdog would have force-stopped it "
echo "within ~1s regardless, as a fail-safe independent of this script.)"
