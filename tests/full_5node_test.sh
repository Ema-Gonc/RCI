#!/bin/bash
set -euo pipefail

cd /Users/ema/Downloads/RCI-deco

pkill -f "owr 127" 2>/dev/null || true
sleep 1
rm -f /tmp/node*_test.log /tmp/owr_monitor.log

# Node 01
(
    echo "dj 517 01"
    sleep 1
    echo "dae 02 127.0.0.1 60002"
    sleep 1
    echo "dae 03 127.0.0.1 60003"
    sleep 1
    echo "sm"
    sleep 1
    echo "sg"
    sleep 1
    echo "a"
    sleep 2
    echo "sr 02"
    sleep 1
    echo "sr 03"
    sleep 1
    echo "sr 04"
    sleep 1
    echo "sr 05"
    sleep 1
    echo "em"
    sleep 1
    echo "x"
) | ./owr 127.0.0.1 60001 > /tmp/node01_test.log 2>&1 &
PID1=$!

# Node 02
(
    echo "dj 517 02"
    sleep 1
    echo "dae 01 127.0.0.1 60001"
    sleep 1
    echo "dae 04 127.0.0.1 60004"
    sleep 1
    echo "sm"
    sleep 1
    echo "sg"
    sleep 1
    echo "a"
    sleep 2
    echo "sr 01"
    sleep 1
    echo "sr 04"
    sleep 1
    echo "sr 05"
    sleep 1
    echo "x"
) | ./owr 127.0.0.1 60002 > /tmp/node02_test.log 2>&1 &
PID2=$!

# Node 03
(
    echo "dj 517 03"
    sleep 1
    echo "dae 01 127.0.0.1 60001"
    sleep 1
    echo "dae 05 127.0.0.1 60005"
    sleep 1
    echo "sm"
    sleep 1
    echo "sg"
    sleep 1
    echo "a"
    sleep 2
    echo "sr 01"
    sleep 1
    echo "sr 05"
    sleep 1
    echo "sr 04"
    sleep 1
    echo "x"
) | ./owr 127.0.0.1 60003 > /tmp/node03_test.log 2>&1 &
PID3=$!

# Node 04
(
    echo "dj 517 04"
    sleep 1
    echo "dae 02 127.0.0.1 60002"
    sleep 1
    echo "dae 05 127.0.0.1 60005"
    sleep 1
    echo "sm"
    sleep 1
    echo "sg"
    sleep 1
    echo "a"
    sleep 2
    echo "sr 01"
    sleep 1
    echo "sr 02"
    sleep 1
    echo "sr 03"
    sleep 1
    echo "x"
) | ./owr 127.0.0.1 60004 > /tmp/node04_test.log 2>&1 &
PID4=$!

# Node 05
(
    echo "dj 517 05"
    sleep 1
    echo "dae 03 127.0.0.1 60003"
    sleep 1
    echo "dae 04 127.0.0.1 60004"
    sleep 1
    echo "sm"
    sleep 1
    echo "sg"
    sleep 1
    echo "a"
    sleep 2
    echo "sr 01"
    sleep 1
    echo "sr 02"
    sleep 1
    echo "sr 03"
    sleep 1
    echo "x"
) | ./owr 127.0.0.1 60005 > /tmp/node05_test.log 2>&1 &
PID5=$!

wait $PID1 $PID2 $PID3 $PID4 $PID5

echo "=== Node logs summary ==="
for i in 1 2 3 4 5; do
    echo "--- node0${i} ---"
    tail -n 20 /tmp/node0${i}_test.log || true
    echo
done
