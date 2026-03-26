# Route Propagation Issues - Root Cause Analysis

## Issue #2: Inconsistent Route Propagation

### Problem Statement
ROUTE messages are not consistently propagated across the overlay network. Example:
- Node 01 knows: Route to 05 with cost 2
- Node 00: Has no route to 05 (even if 01 is a neighbor)

---

## Root Causes Identified

### 🔴 ROOT CAUSE #1: Routes NOT Re-Broadcast After Reception

**Location**: `src/routing.c` - `process_route()` function (line ~135)

```c
void process_route(Node *node, char *neighbor, char *dest, int cost) {
    int new_cost = cost + 1;
    monitor_log("[MONITOR] Received ROUTE from %s: dest=%s cost=%d (new_cost=%d)\n", 
                neighbor, dest, cost, new_cost);
    add_route(node, dest, neighbor, new_cost);  // ❌ NO RE-BROADCAST!
}
```

**The Problem:**
When a node receives a ROUTE message from neighbor, it:
1. ✅ Adds the route to its routing table
2. ✅ Calculates new_cost correctly
3. ❌ **Does NOT broadcast this route to other neighbors**

**Why This Breaks Routing:**
- Node 00 receives ROUTE(05, cost=2) from Node 01
- Node 00 adds this route internally
- Node 00 should announce to **other neighbors** (02, 03, 04): "I have route to 05, cost 3"
- **Currently it doesn't** → Node 02 never learns this route!

**Example Scenario:**
```
Network: 00 -- 01 -- 05
         |
         02

Step 1: 05 announces itself to 01 (self-route, cost 0)
Step 2: 01 broadcasts to neighbors: ROUTE 05 0
Step 3: Node 00 receives ROUTE(05, 0), adds it with cost 1
Step 4: ❌ Node 00 should broadcast to 02: ROUTE 05 1
Step 5: ❌ But it doesn't! Node 02 remains unaware of route to 05
```

---

### 🔴 ROOT CAUSE #2: Missing Initial Neighbor Routes

**Location**: `src/routing.c` - `add_route()` function (line ~51)

```c
void add_route(Node *node, char *dest, char *next, int cost) {
    Route *r = find_route(node, dest);
    if (r) {
        if (cost < r->cost) {
            r->cost = cost;
            strcpy(r->next, next);
        }
    } else {
        // Only creates route if doesn't exist
        // But only on receiving ROUTE messages or explicit operations
    }
}
```

**The Problem:**
When a neighbor connects (edge established):
1. ✅ Node connects to neighbor (TCP handshake)
2. ✅ NEIGHBOR message exchanged
3. ✅ `broadcast_routes()` is called in `o_add_nb()` (line 234 overlay.c)
4. ❌ **But neighbor doesn't automatically have a route back to the connecting node**

**Why This Breaks Routing:**
- Routing assumes **if neighbor exists → route must exist**
- But direct neighbor routes are only created if:
  - The neighbor sends a ROUTE message, OR
  - Routes are propagated from elsewhere
- This violates the routing invariant

---

### 🟡 ROOT CAUSE #3: Inefficient/Incomplete Route Broadcasting Triggers

**Location**: `src/routing.c` - broadcasts only happen at:

1. **Line 234** (`overlay.c`): `o_add_nb()` - When new neighbor joins
   ```c
   broadcast_routes(&my_node);  // Only on edge establishment
   ```

2. **Line 248** (`routing.c`): `check_coordination_end()` - After coordination ends
   ```c
   if (r->cost < INF) broadcast_routes(node);
   ```

3. **Line 309** (`routing.c`): `process_uncoord_msg()` - When UNCOORD received
   ```c
   if (r->cost < INF) {
       broadcast_routes(node);
   }
   ```

**The Problem:**
Routes are NOT re-broadcast when:
- ✅ New routes are received from neighbors (CRITICAL GAP)
- ✅ Existing routes improve in cost
- ✅ Routes become available after coordination failure

**Expected RIP/OSPF Behavior:**
- Any route change → immediately broadcast to all neighbors
- Any new route reception → re-announce to other neighbors (with added cost)

---

### 🟡 ROOT CAUSE #4: Route Cost Comparison Logic

**Location**: `src/routing.c` - `add_route()` (line ~57)

```c
if (cost < r->cost) {  // Only updates if strictly better
    r->cost = cost;
    strcpy(r->next, next);
}
```

**The Problem:**
This only updates if new cost is **strictly less than** existing cost.
- If costs are equal, the route is not updated (even to a better next-hop)
- No mechanism to select better next-hop for same cost

**Impact:**
- Routes can get "stuck" if costs converge prematurely
- No path optimization when multiple equal-cost paths exist

---

## Message Flow Analysis

### Current (Broken) Flow:

```
Node 01 (has route to 05):
  1. broadcast_routes() sends ROUTE 05 0 to neighbors
  
Node 00 (receives from 01):
  2. o_read_nb() receives ROUTE 05 0
  3. process_route() called with dest=05, cost=0, neighbor=01
  4. add_route(00, 05, 01, 1) - adds route with cost 1
  5. ❌ NO BROADCAST - process_route() returns without re-announcing

Node 02 (waiting):
  6. ❌ Node 02 NEVER receives ROUTE 05 message
  7. ❌ No route to 05 in routing table
```

### Expected (Correct) Flow:

```
Node 01: broadcast_routes() sends ROUTE 05 0
Node 00: receives, adds route, then BROADCASTS to neighbors:
         ROUTE 05 1  ← costs should increase by 1
Node 02: receives ROUTE 05 1, adds route with cost 2
```

---

## Additional Issues Blocking Propagation

### Issue A: `send_routes()` vs `broadcast_routes()`

**Location**: `src/routing.c` lines 167-176

```c
void send_routes(Node *node) {
    char buffer[256];
    for (int r = 0; r < node->route_count; r++) {
        sprintf(buffer, "ROUTE %s %d\n", node->routes[r].dest, node->routes[r].cost);
        for (int i = 0; i < MAX_NODES; i++) {
            if (neighbors[i].fd > 0) {
                send_msg(neighbors[i].fd, buffer);  // ✅ Uses send_msg
            }
        }
    }
}
```

vs

```c
void broadcast_routes(Node *node) {
    char msg[256];
    for (int r = 0; r < node->route_count; r++) {
        sprintf(msg, "ROUTE %s %d\n", node->routes[r].dest, node->routes[r].cost);
        for (int i = 0; i < MAX_NODES; i++) {
            if (neighbors[i].fd > 0) {
                write(neighbors[i].fd, msg, strlen(msg));  // ✅ Direct write
            }
        }
    }
}
```

**Problem:** Both functions exist but:
- `send_routes()` is defined but never called
- `broadcast_routes()` is the only one used
- This creates code duplication and confusion
- Neither is called after `process_route()`

---

## Summary: What's Broken

| Scenario | Should Happen | What Actually Happens |
|----------|---------------|----------------------|
| Node receives ROUTE from neighbor | Re-broadcast to other neighbors with +1 cost | NOTHING - no re-broadcast |
| New neighbor connects | Announce self + all routes to new neighbor | ✅ Works (broadcast_routes called) |
| Route improves in cost | Update route + re-broadcast | ❌ Route updates but no re-broadcast |
| Route received during coordination | Queue/process after coordination ends | ✅ Coordination logic OK |

---

## Impact Chain

```
1. Node 00 connects to 01
2. Node 01 has routes that 00 doesn't know about
3. After connect, 00 broadcasts its routes to 01 ✅
4. Node 01 sends ROUTE to 00 ✅
5. Node 00 adds route BUT doesn't re-announce ❌
6. Node 02 (neighbor of 00) never learns of 05 ❌
7. Result: Incomplete routing table across network
```

---

## Recommended Fixes (Priority Order)

### CRITICAL (Breaks core routing):
1. Call `broadcast_routes()` in `process_route()` after successful route addition
   - Send updated routes to ALL neighbors (not just the source)
   
2. Create direct neighbor routes immediately on edge establishment
   - In `o_add_nb()`, explicitly add neighbor routes with cost 1

### IMPORTANT (Improves stability):
3. Replace route update logic to handle:
   - Same-cost alternate paths
   - Better next-hop selection
   
4. Consolidate `send_routes()` and `broadcast_routes()` to use same logic

### NICE-TO-HAVE (Optimization):
5. Add route aging/timeout mechanism
6. Implement split horizon to prevent loops
7. Add triggered updates (send immediately on change, not waiting for periodic broadcast)

