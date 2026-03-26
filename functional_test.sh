#!/bin/bash

# Functional multi-node test using stdin pipes

WORKSPACE="/Users/ema/Downloads/RCI-deco"
cd "$WORKSPACE"

NET=517
BASE_IP="127.0.0.1"

echo "=== Starting 5-Node OWR Automated Test ==="
echo ""

# Kill any existing nodes
pkill -f "owr 127" 2>/dev/null || true
sleep 1

# Function to run a node with stdin commands
run_node() {
    local id=$1
    local port=$2
    local commands=$3
    
    (
        echo "$commands" | ./owr "$BASE_IP" "$port" 
    ) > "/tmp/node_$id.log" 2>&1 &
    
    echo $!
}

echo "[PHASE 1] Starting all nodes..."

# Start all nodes with their commands
CMDS_01="dj $NET 01
sleep 1
dae 02 $BASE_IP 60002
sleep 2
sg
sleep 1
a
sleep 3
sr 05
sleep 2
m 05 Hello from Node 01!
sleep 2
x"

CMDS_02="dj $NET 02
sleep 1
dae 01 $BASE_IP 60001
sleep 0.5
dae 03 $BASE_IP 60003
sleep 0.5
dae 04 $BASE_IP 60004
sleep 2
sg
sleep 5
x"

CMDS_03="dj $NET 03
sleep 1
dae 02 $BASE_IP 60002
sleep 0.5
dae 05 $BASE_IP 60005
sleep 2
sg
sleep 5
x"

CMDS_04="dj $NET 04
sleep 1
dae 02 $BASE_IP 60002
sleep 2
sg
sleep 5
x"

CMDS_05="dj $NET 05
sleep 1
dae 03 $BASE_IP 60003
sleep 2
sg
sleep 1
a
sleep 3
sr 01
sleep 2
m 01 Hello from Node 05!
sleep 2
x"

echo "  Starting Node 01..."
PID1=$(run_node "01" "60001" "$CMDS_01")

sleep 0.3

echo "  Starting Node 02..."
PID2=$(run_node "02" "60002" "$CMDS_02")

sleep 0.3

echo "  Starting Node 03..."
PID3=$(run_node "03" "60003" "$CMDS_03")

sleep 0.3

echo "  Starting Node 04..."
PID4=$(run_node "04" "60004" "$CMDS_04")

sleep 0.3

echo "  Starting Node 05..."
PID5=$(run_node "05" "60005" "$CMDS_05")

echo ""
echo "Waiting for test to complete..."
wait $PID1 2>/dev/null
wait $PID2 2>/dev/null
wait $PID3 2>/dev/null
wait $PID4 2>/dev/null
wait $PID5 2>/dev/null

echo ""
echo "=== Test Complete ==="
echo ""
echo "Full logs:"
echo ""

for i in 01 02 03 04 05; do
    echo "====== Node $i ======"
    cat "/tmp/node_$i.log"
    echo ""
done
