#include "../include/routing.h"
#include "../include/common.h"
#include "../include/net.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define STATE_EXPEDITION 0
#define STATE_COORDINATION 1
#define INF 999

// External neighbors array managed by overlay.c
extern Neighbor neighbors[MAX_NODES];

// External config for monitoring
extern AppConfig config;

// Prototypes
void check_coordination_end(Node *node, Route *r);

// Helper function to log routing messages if monitoring enabled
static void monitor_log(const char *format, ...) {
    extern AppConfig config;
    if (!config.monitor) return;

    va_list args;
    va_start(args, format);
    vprintf(format, args);

    FILE *f = fopen("/tmp/owr_monitor.log", "a");
    if (f) {
        va_list args2;
        va_copy(args2, args);
        vfprintf(f, format, args2);
        va_end(args2);
        fclose(f);
    }

    va_end(args);
}

// Helper function to send message
// int send_msg(int fd, char *msg) {
//     return write(fd, msg, strlen(msg));
// }

// Helper function to find route by dest
Route *find_route(Node *node, char *dest) {
    for (int i = 0; i < node->route_count; i++) {
        if (strcmp(node->routes[i].dest, dest) == 0) {
            return &node->routes[i];
        }
    }
    return NULL;
}

// Helper function to add or update route
int add_route(Node *node, char *dest, char *next, int cost) {
    Route *r = find_route(node, dest);
    if (r) {
        if (cost < r->cost) {
            r->cost = cost;
            strcpy(r->next, next);
            
            // If route was in coordination (INF cost) and now has a valid path,
            // recover it back to EXPEDITION state
            if (r->state == STATE_COORDINATION && cost < INF) {
                r->state = STATE_EXPEDITION;
                strcpy(r->succ_coord, "-1");
            }
            return 1;  // Route was updated
        }
        return 0;  // No change (cost not better)
    } else {
        if (node->route_count < MAX_NODES) {
            strcpy(node->routes[node->route_count].dest, dest);
            node->routes[node->route_count].cost = cost;
            strcpy(node->routes[node->route_count].next, next);
            node->routes[node->route_count].state = STATE_EXPEDITION;
            strcpy(node->routes[node->route_count].succ_coord, "-1");
            memset(node->routes[node->route_count].coord_pending, 0, sizeof(node->routes[node->route_count].coord_pending));
            node->route_count++;
            return 1;  // New route added
        }
        return 0;  // No space for new route
    }
}

// Initialize node
void node_init(Node *node, char *id, const char *ip, int port) {
    strcpy(node->id, id);
    (void)ip; (void)port; // Suppress unused warnings
    node->neighbor_count = 0;
    node->route_count = 0;
    // Add self-route
    add_route(node, id, id, 0);
}

// Add neighbor to node (sync with overlay neighbors)
void add_neighbor(Node *node, char *id, char *ip, int port) {
    (void)node; (void)id; (void)ip; (void)port; // Suppress unused warnings
    // This is handled in overlay.c, but we can sync
    // For now, assume neighbors are managed externally
}

// Get next hop for destination
char *get_next_hop(Node *node, char *dest) {
    Route *r = find_route(node, dest);
    if (r) {
        return r->next;
    }
    return NULL;
}

// Print routes
void print_routes(Node *node) {
    printf("Tabela de rotas:\n");
    for (int i = 0; i < node->route_count; i++) {
        const char *estado = (node->routes[i].state == STATE_EXPEDITION) ? "expedição" : "coordenação";
        printf("Destino: %s | Próximo salto: %s | Custo: %d | Estado: %s\n",
               node->routes[i].dest,
               node->routes[i].next,
               node->routes[i].cost,
               estado);
    }
}

// Send chat message
void send_chat(int sock, char *dest, char *msg) {
    char buffer[512];
    sprintf(buffer, "CHAT %s %s\n", dest, msg);
    monitor_log("[MONITOR] Send CHAT to %s: %s\n", dest, msg);
    write(sock, buffer, strlen(buffer));
}

// Send route update
void send_route_update(int sock, char *dest, int cost) {
    char buffer[256];
    sprintf(buffer, "ROUTE %s %d\n", dest, cost);
    monitor_log("[MONITOR] Send ROUTE %s cost=%d\n", dest, cost);
    write(sock, buffer, strlen(buffer));
}

void process_route(Node *node, char *neighbor, char *dest, int cost) {
    int new_cost = cost + 1;
    monitor_log("[MONITOR] Received ROUTE from %s: dest=%s cost=%d (new_cost=%d)\n", neighbor, dest, cost, new_cost);
    
    // FIX: Accept routes even during coordination (needed for recovery)
    // This allows discovery of alternate paths when primary path fails
    // Find the route to see if we can use this for recovery
    Route *r = find_route(node, dest);
    int was_in_coordination = (r && r->state == STATE_COORDINATION) ? 1 : 0;
    
    int route_changed = add_route(node, dest, neighbor, new_cost);
    
    // CRITICAL FIX: Only broadcast if the route table actually changed
    // This prevents infinite loops where nodes keep broadcasting unchanged routes
    if (route_changed) {
        // If this route was in coordination and we found an alternate path, we recovered!
        // Re-broadcast all routes to propagate the learned route to other neighbors
        // This ensures the routing information spreads throughout the network
        broadcast_routes(node);
        
        // FIX D: If we recovered from coordination through alternate path, we may need to
        // send UNCOORD to nodes waiting for this route's recovery
        if (was_in_coordination && r && r->state == STATE_EXPEDITION && r->cost < INF) {
            monitor_log("[MONITOR] Route %s recovered during coordination (new cost=%d)\n", dest, r->cost);
            // The add_route() function already set state back to EXPEDITION if recovered
            // and we just broadcast it, so recovery process is complete
        }
    }
}

void broadcast_routes(Node *node) {
    char msg[256];
    for (int r = 0; r < node->route_count; r++) {
        // FIX: Only broadcast routes in EXPEDITION state
        // Routes in COORDINATION (with cost=INF) should NOT be broadcast
        if (node->routes[r].state != STATE_EXPEDITION) {
            continue;  // Skip routes not in expedition state
        }
        // Also skip routes with infinite cost (should never happen in EXPEDITION, but be safe)
        if (node->routes[r].cost >= INF) {
            continue;
        }
        
        sprintf(msg, "ROUTE %s %d\n", node->routes[r].dest, node->routes[r].cost);
        monitor_log("[MONITOR] Broadcast ROUTE %s cost=%d (state=EXPEDITION) to all neighbors\n", node->routes[r].dest, node->routes[r].cost);
        // Send to all active neighbors in the global neighbors array
        for (int i = 0; i < MAX_NODES; i++) {
            if (neighbors[i].fd > 0) {
                write(neighbors[i].fd, msg, strlen(msg));
            }
        }
    }
}

void send_routes(Node *node) {
    char buffer[256];
    for (int r = 0; r < node->route_count; r++) {
        sprintf(buffer, "ROUTE %s %d\n", node->routes[r].dest, node->routes[r].cost);
        // Send to all active neighbors in the global neighbors array
        for (int i = 0; i < MAX_NODES; i++) {
            if (neighbors[i].fd > 0) {
                send_msg(neighbors[i].fd, buffer);
            }
        }
    }
}

void process_route_update(Node *node, char *neighbor, char *dest, int cost) {
    int new_cost = cost + 1;
    add_route(node, dest, neighbor, new_cost);
}

void process_coord(Node *node, char *j, char *t) {
    Route *r = find_route(node, t);
    if (!r) return;

    if (r->state == STATE_COORDINATION) {
        send_uncoord(node, j, t);
    } else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) != 0) {
        send_route_to_id(node, j, t, r->cost);
        send_uncoord(node, j, t);
    } else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) == 0) {
        r->state = STATE_COORDINATION;
        strcpy(r->succ_coord, r->next);
        r->cost = INF;
        strcpy(r->next, "-1");

        // Broadcast COORD to all active neighbors
        for (int i = 0; i < MAX_NODES; i++) {
            if (neighbors[i].fd > 0) {
                send_coord(node, neighbors[i].id, t);
            }
        }
    }
}

void handle_edge_failure(Node *node, char *neighbor_id) {
    for (int i = 0; i < node->route_count; i++) {
        Route *r = &node->routes[i];
        if (strcmp(r->next, neighbor_id) == 0) {
            if (r->state == STATE_EXPEDITION) {
                r->state = STATE_COORDINATION;
                strcpy(r->succ_coord, "-1");
                r->cost = INF;
                strcpy(r->next, "-1");

                // Broadcast COORD to all neighbors except the failed one
                for (int j = 0; j < MAX_NODES; j++) {
                    if (neighbors[j].fd > 0 && strcmp(neighbors[j].id, neighbor_id) != 0) {
                        send_coord(node, neighbors[j].id, r->dest);
                    }
                }
                check_coordination_end(node, r);
            }
        }
    }
}

void process_coord_msg(Node *node, char *j, char *t) {
    Route *r = find_route(node, t);
    if (!r) return;
    monitor_log("[MONITOR] Received COORD from %s: dest=%s (my state=%s)\n", j, t, (r->state == STATE_EXPEDITION) ? "EXPEDITION" : "COORDINATION");
    if (r->state == STATE_COORDINATION) {
        send_uncoord(node, j, t);
    } else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) != 0) {
        send_route_to_id(node, j, t, r->cost);
        send_uncoord(node, j, t);
    } else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) == 0) {
        r->state = STATE_COORDINATION;
        strcpy(r->succ_coord, j);
        r->cost = INF;
        strcpy(r->next, "-1");

        // Broadcast COORD to all active neighbors
        for (int k = 0; k < MAX_NODES; k++) {
            if (neighbors[k].fd > 0) {
                send_coord(node, neighbors[k].id, t);
            }
        }
    }
}

void send_coord(Node *node, char *target_id, char *dest) {
    char msg[128];
    sprintf(msg, "COORD %s\n", dest);
    monitor_log("[MONITOR] Send COORD %s to node %s\n", dest, target_id);
    for (int i = 0; i < MAX_NODES; i++) {
        if (neighbors[i].fd > 0 && strcmp(neighbors[i].id, target_id) == 0) {
            send_msg(neighbors[i].fd, msg);
        }
    }
    (void)node; // Suppress unused warning
}

void send_uncoord(Node *node, char *target_id, char *dest) {
    char msg[128];
    sprintf(msg, "UNCOORD %s\n", dest);
    monitor_log("[MONITOR] Send UNCOORD %s to node %s\n", dest, target_id);
    for (int i = 0; i < MAX_NODES; i++) {
        if (neighbors[i].fd > 0 && strcmp(neighbors[i].id, target_id) == 0) {
            send_msg(neighbors[i].fd, msg);
        }
    }
    (void)node; // Suppress unused warning
}

void send_route_to_id(Node *node, char *target_id, char *dest, int cost) {
    char msg[128];
    sprintf(msg, "ROUTE %s %d\n", dest, cost);

    for (int i = 0; i < MAX_NODES; i++) {
        if (neighbors[i].fd > 0 && strcmp(neighbors[i].id, target_id) == 0) {
            send_msg(neighbors[i].fd, msg);
            return;
        }
    }
    (void)node; // Suppress unused warning
}

void check_coordination_end(Node *node, Route *r) {
    // Check if this route is still being coordinated
    if (r->state == STATE_COORDINATION && r->cost == INF) {
        // Coordination not yet complete, still in progress
        return;
    }
    r->state = STATE_EXPEDITION;
    if (r->cost != INF) broadcast_routes(node);
    if (strcmp(r->succ_coord, "-1") != 0) send_uncoord(node, r->succ_coord, r->dest);
}

void process_uncoord_msg(Node *node, char *j, char *t) {
    Route *r = find_route(node, t);
    if (!r) return;
    
    (void)j; // Suppress unused warning
    
    monitor_log("[MONITOR] Received UNCOORD from neighbor: dest=%s\n", t);

    // If we're in coordination state with an UNCOORD message, we're done coordinating
    if (r->state == STATE_COORDINATION) {
        // Check if there's any more cost/route available
        if (r->cost == INF) {
            // Still infinite, no recovery possible
            r->state = STATE_EXPEDITION;
        } else {
            // Re-enter expedition with current cost
            r->state = STATE_EXPEDITION;
        }
        
        // printf("Node returned to expedition state for destination %s\n", t);
        if (r->cost < INF) {
            broadcast_routes(node);
        }
        if (strcmp(r->succ_coord, "-1") != 0) {
            send_uncoord(node, r->succ_coord, t);
        }
    }
}
