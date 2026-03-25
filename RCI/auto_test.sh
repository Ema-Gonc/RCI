#!/bin/bash

# Automated multi-node testing script for OWR distributed routing system
# Uses expect to interact with nodes via stdin/stdout

WORKSPACE="/Users/ema/Downloads/RCI-deco"
cd "$WORKSPACE"

NET=517
BASE_IP="127.0.0.1"

echo "=== Starting 5-Node OWR Test ==="
echo ""

# Check if expect is installed
if ! command -v expect &> /dev/null; then
    echo "Error: expect is required for this test script"
    echo "Install with: brew install expect"
    exit 1
fi

# Phase 1: Start nodes
echo "[PHASE 1] Starting nodes..."

# Node 01
expect -c "
spawn ./owr $BASE_IP 60001
expect \"commands\"
send \"dj $NET 01\r\"
expect \"Success\"
send \"dae 02 $BASE_IP 60002\r\"
sleep 1
send \"sg\r\"
sleep 1
send \"a\r\"
sleep 2
send \"sr 05\r\"
sleep 1
send \"m 05 Hello from Node 01 to Node 05!\r\"
sleep 2
send \"x\r\"
expect \"Goodbye\"
" > /tmp/node_01.log 2>&1 &
PID1=$!

sleep 0.5

# Node 02
expect -c "
spawn ./owr $BASE_IP 60002
expect \"commands\"
send \"dj $NET 02\r\"
expect \"Success\"
send \"dae 01 $BASE_IP 60001\r\"
sleep 0.5
send \"dae 03 $BASE_IP 60003\r\"
sleep 0.5
send \"dae 04 $BASE_IP 60004\r\"
sleep 1
send \"sg\r\"
sleep 2
send \"x\r\"
expect \"Goodbye\"
" > /tmp/node_02.log 2>&1 &
PID2=$!

sleep 0.5

# Node 03
expect -c "
spawn ./owr $BASE_IP 60003
expect \"commands\"
send \"dj $NET 03\r\"
expect \"Success\"
send \"dae 02 $BASE_IP 60002\r\"
sleep 0.5
send \"dae 05 $BASE_IP 60005\r\"
sleep 1
send \"sg\r\"
sleep 1
send \"x\r\"
expect \"Goodbye\"
" > /tmp/node_03.log 2>&1 &
PID3=$!

sleep 0.5

# Node 04
expect -c "
spawn ./owr $BASE_IP 60004
expect \"commands\"
send \"dj $NET 04\r\"
expect \"Success\"
send \"dae 02 $BASE_IP 60002\r\"
sleep 1
send \"sg\r\"
sleep 1
send \"x\r\"
expect \"Goodbye\"
" > /tmp/node_04.log 2>&1 &
PID4=$!

sleep 0.5

# Node 05
expect -c "
spawn ./owr $BASE_IP 60005
expect \"commands\"
send \"dj $NET 05\r\"
expect \"Success\"
send \"dae 03 $BASE_IP 60003\r\"
sleep 1
send \"a\r\"
sleep 2
send \"sr 01\r\"
sleep 1
send \"m 01 Hello from Node 05 to Node 01!\r\"
sleep 2
send \"x\r\"
expect \"Goodbye\"
" > /tmp/node_05.log 2>&1 &
PID5=$!

# Wait for all nodes to complete
echo "Waiting for all nodes to complete..."
wait $PID1 2>/dev/null
wait $PID2 2>/dev/null
wait $PID3 2>/dev/null
wait $PID4 2>/dev/null
wait $PID5 2>/dev/null

echo ""
echo "=== Test Complete ==="
echo ""
echo "Results from each node:"
for i in 01 02 03 04 05; do
    echo ""
    echo "--- Node $i ---"
    cat /tmp/node_$i.log | grep -E "(registered|Neighbors|Dest:|Chat|Routing|Success|Error)" || echo "(no filtered output)"
done
