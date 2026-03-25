#!/bin/bash

# Proper multi-node test with bash command execution

WORKSPACE="/Users/ema/Downloads/RCI-deco"
cd "$WORKSPACE"

NET=517
BASE_IP="127.0.0.1"

echo "=== OWR 5-Node Automated Test ==="
echo ""

# Kill any existing nodes  
pkill -f "owr 127" 2>/dev/null || true
sleep 1

run_node() {
    local id=$1
    local port=$2
    
    {
        sleep 2  # Wait for listener to start
        
        # Direct join
        echo "dj $NET $id"
        sleep 1
        
        # Show neighbors (should be empty at start)
        echo "sg"
        
        # Exit cleanly
        sleep 10
        echo "x"
        
    } | ./owr "$BASE_IP" "$port"
}

echo "[PHASE 1] Starting all nodes..."
echo ""

# Start all 5 nodes simultaneously
echo "  Starting nodes 01-05..."
(run_node "01" "60001" > /tmp/node_01.log 2>&1) &
(run_node "02" "60002" > /tmp/node_02.log 2>&1) &
(run_node "03" "60003" > /tmp/node_03.log 2>&1) &
(run_node "04" "60004" > /tmp/node_04.log 2>&1) &
(run_node "05" "60005" > /tmp/node_05.log 2>&1) &

# Give them all time to start
sleep 15

echo ""
echo "[PHASE 2] Starting connection test..."
echo ""

# Now test with manual connections in separate instances
{
    sleep 1
    echo "dj $NET 01"
    sleep 1
    echo "dae 02 $BASE_IP 60002"
    sleep 2
    echo "sg"
    sleep 2
    echo "x"
} | ./owr "$BASE_IP" "60010" > /tmp/test_01.log 2>&1 &

sleep 2

{
    sleep 1
    echo "dj $NET 02"
    sleep 1
    echo "dae 01 $BASE_IP 60010"
    sleep 2
    echo "sg"
    sleep 2
    echo "x"
} | ./owr "$BASE_IP" "60011" > /tmp/test_02.log 2>&1 &

wait

echo ""
echo "=== Test Output ==="
echo ""
echo "Test Node 01:"
cat /tmp/test_01.log | grep -v "^Connecting to Node Server"
echo ""
echo "Test Node 02:"
cat /tmp/test_02.log | grep -v "^Connecting to Node Server"
