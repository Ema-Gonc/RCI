# Audit Checklist - 4 Critical Requirements

## ✅/❌ Requirement 1: Direct Neighbor Always dist=1

**Expected**: When neighbor connects → automatic route with cost 1 to that neighbor

**Status**: ✅ IMPLEMENTED (FIX #2)
- **Location**: `src/overlay.c` - `o_add_nb()` line ~237
- **Code**: `add_route(&my_node, neighbor_id, neighbor_id, 1);`
- **Result**: Route created immediately upon connection

---

## ⚠️ Requirement 2: Broadcast After Better Route in EXPEDITION

**Expected**: 
```c
if (state == EXPEDITION && received_better_route)
    broadcast ROUTE
```

**Status**: ⚠️ PARTIALLY CORRECT (Issue Found)

**What We Have**:
- `process_route()` calls `broadcast_routes()` after adding route ✅
- But `broadcast_routes()` broadcasts **ALL routes** regardless of state ❌
- **Problem**: Routes in COORDINATION state (with cost=INF) are being broadcast!

**Code Issue** (`src/routing.c` line ~150):
```c
void broadcast_routes(Node *node) {
    for (int r = 0; r < node->route_count; r++) {
        sprintf(msg, "ROUTE %s %d\n", node->routes[r].dest, node->routes[r].cost);
        // ❌ Broadcasts even if routes[r].state == STATE_COORDINATION
        // ❌ Broadcasts even if routes[r].cost == INF (999)
        for (int i = 0; i < MAX_NODES; i++) {
            if (neighbors[i].fd > 0) {
                write(neighbors[i].fd, msg, strlen(msg));
            }
        }
    }
}
```

**Impact**: 
- Nodes receive `ROUTE dest 999` messages
- This taints routing tables with infinite costs
- Breaks convergence for that destination

---

## ❌ Requirement 3: Coordination Termination Recovery

**Expected**:
```
After UNCOORD received (when all coord[t] == 0):
1. Switch state to EXPEDITION
2. Re-broadcast ROUTE messages
3. Return to normal operation
```

**Status**: ❌ PARTIALLY CORRECT (Issue Found)

**What We Have** (`src/routing.c` ~305-320):
```c
void process_uncoord_msg(Node *node, char *j, char *t) {
    Route *r = find_route(node, t);
    if (!r) return;
    
    if (r->state == STATE_COORDINATION) {
        // ✅ Sets state back to EXPEDITION
        r->state = STATE_EXPEDITION;
        
        // ✅ Broadcasts routes if cost is not INF
        if (r->cost < INF) {
            broadcast_routes(node);
        }
        
        // ✅ Sends UNCOORD to successor
        if (strcmp(r->succ_coord, "-1") != 0) {
            send_uncoord(node, r->succ_coord, t);
        }
    }
}
```

**But There's An Issue**:
- `broadcast_routes()` still broadcasts routes in COORD state (see issue #2)
- If we just recovered a lost route (cost was INF), we need to **immediately** have it at new cost
  - But when does this new cost get set? It should come from `add_route()` call when we receive route
  - **Problem**: During coordination, we ignore incoming ROUTE messages! 
  - **Result**: When coordination ends, we still have no valid route
  - **Impact**: Route stays "lost" until next periodic broadcast (no mechanism exists)

---

## ❌ Requirement 4: TCP Stability Issues

### Issue 4a: Double Close Detection

**Location**: `src/overlay.c` - `o_add_nb()` line ~218 and duplicate edge handling line ~276

**Code Problem**:
```c
if (neighbors[slot].fd != -1 && neighbors[slot].fd != fd)
    close(neighbors[slot].fd);  // ✅ Checks fd != -1 before close
```
**This is OK** ✅

**But line 276 has a problem**:
```c
} else {
    // Duplicate edge to same neighbor: keep current and close redundant one
    close(neighbors[existing_slot].fd);  // ❌ Doesn't check if fd is -1!
    neighbors[existing_slot].fd = -1;
    strcpy(neighbors[existing_slot].id, "-1");
    // ❌ Doesn't clear ip, tcp buffers
    sprintf(neighbors[i].id, "%02d", id);
}
```

**Problem**: 
- If `neighbors[existing_slot].fd` was already -1, this closes an invalid FD
- Should use `clear_slot()` instead for consistency
- Leaves ip/tcp fields in dirty state

### Issue 4b: Read == 0 Handling

**Location**: `src/overlay.c` - `o_read_nb()` line ~257

**Code**:
```c
if (n == -1) {
    perror("Error reading from neighbor socket");
    clear_slot(i);  // ✅ Proper cleanup
} else if (n == 0) {
    printf("Neighbor %s disconnected. Closing edge (FD: %d)\n",
           neighbors[i].id, fd);
    clear_slot(i);  // ✅ Proper cleanup
}
```

**Status**: ✅ CORRECT - read() == 0 is properly handled

### Issue 4c: FD Reuse Safety

**Status**: ⚠️ POTENTIAL ISSUE

When we close a socket and set fd = -1, we should ensure no other code tries to use it. Looking at the code:
- `broadcast_routes()` checks `if (neighbors[i].fd > 0)` ✅ Good
- `send_coord/uncoord()` checks `if (neighbors[i].fd > 0)` ✅ Good
- But line 276 doesn't check before close ❌

---

## Summary Table

| Requirement | Status | Issue Severity | Lines |
|---|---|---|---|
| Neighbor dist=1 | ✅ OK | None | overlay.c:237 |
| Broadcast on better route | ⚠️ BROKEN | 🔴 HIGH | routing.c:150 |
| State filter in broadcast | ❌ MISSING | 🔴 HIGH | routing.c:150 |
| Coordination recovery | ⚠️ PARTIAL | 🟡 MEDIUM | routing.c:305 |
| Routes during COORD ignored | ❌ BY DESIGN | 🟡 MEDIUM | routing.c:135 |
| Double close on duplicate edge | ❌ YES | 🟡 MEDIUM | overlay.c:276 |
| TCP read() == 0 | ✅ OK | None | overlay.c:257 |

---

## Critical Fixes Needed

### FIX A (HIGH PRIORITY): Filter broadcast by state
- Only broadcast routes where `state == STATE_EXPEDITION`
- Never broadcast routes with `cost == INF`
- Prevents tainting routing tables

### FIX B (HIGH PRIORITY): Don't ignore routes during coordination
- In `process_route()`, still accept new routes even in COORD state
- Allows recovery path discovery during coordination

### FIX C (MEDIUM PRIORITY): Use clear_slot for duplicate edge
- Replace manual close()/set/strcpy with `clear_slot()`
- Prevents double close, ensures consistency

### FIX D (MEDIUM PRIORITY): Handle routes in coordination during termination
- When coordination ends, ensure we have a valid route
- If no valid route found, don't stay in EXPEDITION with INF cost

---

## Node Recovery in Loop Topology

**Question**: Can a node recover when removed from a loop?

**Current Answer**: ❌ NO - Routes are not being properly broadcast due to state filtering issue

**Requirements for recovery**:
1. ✅ Node detects neighbor disconnected (handled_edge_failure called)
2. ✅ Route enters COORDINATION 
3. ❌ Alternative routes NOT accepted during COORDINATION (rules-based)
4. ❌ When coordination ends, no mechanism to find new route
5. Route stays lost until next edge change

**Example scenario**:
```
Loop: 00 ↔ 01 ↔ 02 ↔ 00

Node 00 has routes:
- 01: cost 1 (direct)
- 02: cost 2 (via 01)

Break edge 00↔01:
- Route to 01 → COORD (cost INF, next -1)
- Route to 02 → COORD (cost INF, next -1)

Node 00 should receive from 02:
- "ROUTE 01 1" (02 can reach 01)
- But currently IGNORED because 00 is in COORD for 01

Result: Route never recovered until different topology change
```

**To fix**: Need FIX B (accept routes during COORD) or trigger broadcast when COORD ends

