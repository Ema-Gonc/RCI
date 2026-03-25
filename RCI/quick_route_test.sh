#!/bin/bash

# Quick 2-node test to verify routes are broadcast correctly

cd /Users/ema/Downloads/RCI-deco

echo "=== 2-Node Route Broadcast Test ==="
echo ""

pkill -f "owr 127" 2>/dev/null || true
sleep 1

echo "Starting Node 01 on port 60001..."
(
    sleep 1
    echo "dj 517 01"
    sleep 1
    echo "dae 02 127.0.0.1 60002"
    sleep 3
    echo "sg"
    sleep 1
    echo "a"
    sleep 2
    echo "sr 02"
    sleep 1
    echo "sr 01"
    sleep 1
    echo "x"
) | ./owr 127.0.0.1 60001 > /tmp/node01_test.log 2>&1 &
PID1=$!

sleep 1

echo "Starting Node 02 on port 60002..."
(
    sleep 2
    echo "dj 517 02"
    sleep 1
    echo "dae 01 127.0.0.1 60001"
    sleep 3
    echo "sg"
    sleep 1
    echo "a"
    sleep 2
    echo "sr 01"
    sleep 1
    echo "sr 02"
    sleep 1
    echo "x"
) | ./owr 127.0.0.1 60002 > /tmp/node02_test.log 2>&1 &
PID2=$!

wait $PID1
wait $PID2

echo ""
echo "=== Node 01 Logs ==="
cat /tmp/node01_test.log

echo ""
echo "=== Node 02 Logs ==="
cat /tmp/node02_test.log

echo ""
echo "=== Analysis ==="
echo "Checking for ROUTE messages and routing table entries..."
echo ""

if grep -q "Rota para 02" /tmp/node01_test.log; then
    echo "✓ Node 01 tem rota para Node 02"
else
    echo "✗ Node 01 sem rota para Node 02"
fi

if grep -q "Rota para 01" /tmp/node02_test.log; then
    echo "✓ Node 02 tem rota para Node 01"
else
    echo "✗ Node 02 sem rota para Node 01"
fi

echo ""
