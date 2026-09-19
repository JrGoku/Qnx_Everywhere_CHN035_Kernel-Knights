#!/bin/sh
# run_parking.sh - launches the multi-process parking system
# (motor_proc + decision_proc + sensor_proc), detached from the current
# shell via nohup so it survives an SSH/Ethernet disconnect - same
# untethered-demo approach used for parking_app.
#
# Order does NOT matter for correctness: ipc_client_connect() inside
# decision_proc/sensor_proc retries until the server it needs is up, so
# these can even be started in the opposite order. Starting the two
# servers (motor_proc, decision_proc) first here just avoids a moment of
# retry-logging on the very first run.
#
# Usage: ./run_parking.sh
# Stop:  ./stop_parking.sh

cd "$(dirname "$0")" || exit 1

PIDFILE=/tmp/parking_pids
: > "$PIDFILE"

echo "Starting motor_proc..."
nohup ./motor_proc > motor_proc.log 2>&1 &
echo $! >> "$PIDFILE"

echo "Starting decision_proc..."
nohup ./decision_proc > decision_proc.log 2>&1 &
echo $! >> "$PIDFILE"

echo "Starting sensor_proc..."
nohup ./sensor_proc > sensor_proc.log 2>&1 &
echo $! >> "$PIDFILE"

disown -a 2>/dev/null

echo ""
echo "All three processes started. PIDs saved to $PIDFILE:"
cat "$PIDFILE"
echo ""
echo "Per-process console logs (mirrors of what's in the system log too):"
echo "  motor_proc.log  decision_proc.log  sensor_proc.log"
echo "Full system history (survives any of these processes crashing/exiting):"
echo "  sloginfo"
echo ""
echo "To stop everything safely: ./stop_parking.sh"
