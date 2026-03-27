#include "../include/routing.h"
#include "../include/common.h"

#include <stdio.h>
#include <string.h>

#define STATE_EXPEDITION 0
#define STATE_COORDINATION 1
#define INF 999

extern Neighbor neighbors[MAX_NODES];

static void check_coordination_end(Node *node, Route *r);
static int find_neighbor_slot_by_id(const char *id);
static void set_text(char *dest, size_t size, const char *src);

static void set_text(char *dest, size_t size, const char *src) {
  if (size == 0)
    return;
  if (src == NULL)
    src = "";
  snprintf(dest, size, "%s", src);
}

static int find_neighbor_slot_by_id(const char *id) {
  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd != -1 && strcmp(neighbors[i].id, id) == 0) {
      return i;
    }
  }
  return -1;
}

Route *find_route(Node *node, char *dest) {
  for (int i = 0; i < node->route_count; i++) {
    if (strcmp(node->routes[i].dest, dest) == 0) {
      return &node->routes[i];
    }
  }
  return NULL;
}

int add_route(Node *node, char *dest, char *next, int cost) {
  Route *r = find_route(node, dest);
  if (r) {
    if (cost < r->cost) {
      r->cost = cost;
      set_text(r->next, sizeof(r->next), next);
      if (r->state == STATE_COORDINATION && cost < INF) {
        r->state = STATE_EXPEDITION;
        set_text(r->succ_coord, sizeof(r->succ_coord), "-1");
      }
      return 1;
    }
    return 0;
  } else {
    if (node->route_count < MAX_NODES) {
      Route *new_route = &node->routes[node->route_count];
      set_text(new_route->dest, sizeof(new_route->dest), dest);
      new_route->cost = cost;
      set_text(new_route->next, sizeof(new_route->next), next);
      new_route->state = STATE_EXPEDITION;
      set_text(new_route->succ_coord, sizeof(new_route->succ_coord), "-1");
      memset(new_route->coord_pending, 0, sizeof(new_route->coord_pending));
      node->route_count++;
      return 1;
    }
    return 0;
  }
}

void node_init(Node *node, char *id, const char *ip, int port) {
  int numeric_id;
  char formatted_id[3];
  if (sscanf(id, "%d", &numeric_id) == 1 && format_id(formatted_id, numeric_id) == 0) {
    set_text(node->id, sizeof(node->id), formatted_id);
  } else {
    set_text(node->id, sizeof(node->id), id);
  }
  (void)ip;
  (void)port;
  node->neighbor_count = 0;

  node->route_count = MAX_NODES;
  for (int i = 0; i < MAX_NODES; i++) {
    char dest_id[3];
    format_id(dest_id, i);
    set_text(node->routes[i].dest, sizeof(node->routes[i].dest), dest_id);
    node->routes[i].cost = INF;
    set_text(node->routes[i].next, sizeof(node->routes[i].next), "-1");
    node->routes[i].state = STATE_EXPEDITION;
    set_text(node->routes[i].succ_coord, sizeof(node->routes[i].succ_coord), "-1");
    memset(node->routes[i].coord_pending, 0, sizeof(node->routes[i].coord_pending));
  }
}

void add_neighbor(Node *node, char *id, char *ip, int port) {
  (void)node;
  (void)id;
  (void)ip;
  (void)port;
}

char *get_succ(Node *node, char *dest) {
  Route *r = find_route(node, dest);
  if (r) {
    return r->next;
  }
  return NULL;
}

void print_routes(Node *node) {
  printf("Tabela de rotas:\n");
  for (int i = 0; i < node->route_count; i++) {
    const char *estado = (node->routes[i].state == STATE_EXPEDITION)
                             ? "expedição"
                             : "coordenação";
    printf("Destino: %s | Próximo salto: %s | Custo: %d | Estado: %s\n",
           node->routes[i].dest, node->routes[i].next, node->routes[i].cost,
           estado);
  }
}

void send_chat(int sock, char *dest, char *msg) {
  char buffer[512];
  char clipped_msg[129];
  strncpy(clipped_msg, msg, 128);
  clipped_msg[128] = '\0';

  snprintf(buffer, sizeof(buffer), "CHAT %s %s\n", dest, clipped_msg);
  monitor_log("[MONITOR] TX: %s", buffer);
  send_msg(sock, buffer);
}

void send_route_update(int sock, char *dest, int cost) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "ROUTE %s %d\n", dest, cost);
  monitor_log("[MONITOR] TX: %s", buffer);
  send_msg(sock, buffer);
}

void process_route(Node *node, char *neighbor, char *dest, int cost) {
  int new_cost = cost + 1;

  Route *r = find_route(node, dest);
  int was_in_coordination = (r && r->state == STATE_COORDINATION) ? 1 : 0;

  int route_changed = add_route(node, dest, neighbor, new_cost);
  if (route_changed && find_route(node, dest)->state == STATE_EXPEDITION) {
    broadcast_routes(node);
  }

  if (route_changed && was_in_coordination) {
    Route *updated = find_route(node, dest);
    if (updated && updated->cost < INF) {
      monitor_log("[MONITOR] Route %s updated during coordination (new cost=%d)\n",
                  dest, updated->cost);
    }
  }
}

void broadcast_routes(Node *node) {
  char msg[256];
  for (int r = 0; r < node->route_count; r++) {
    if (node->routes[r].state != STATE_EXPEDITION) {
      continue;
    }
    if (node->routes[r].cost >= INF) {
      continue;
    }

    snprintf(msg, sizeof(msg), "ROUTE %s %d\n", node->routes[r].dest,
             node->routes[r].cost);

    for (int i = 0; i < MAX_NODES; i++) {
      if (neighbors[i].fd > 0) {
        monitor_log("[MONITOR] TX to %s: %s", neighbors[i].id, msg);
        send_msg(neighbors[i].fd, msg);
      }
    }
  }
}

void send_routes(Node *node) {
  char buffer[256];
  for (int r = 0; r < node->route_count; r++) {
    snprintf(buffer, sizeof(buffer), "ROUTE %s %d\n", node->routes[r].dest,
             node->routes[r].cost);

    for (int i = 0; i < MAX_NODES; i++) {
      if (neighbors[i].fd > 0) {
        monitor_log("[MONITOR] TX to %s: %s", neighbors[i].id, buffer);
        send_msg(neighbors[i].fd, buffer);
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
        set_text(r->succ_coord, sizeof(r->succ_coord), "-1");
        r->cost = INF;
        set_text(r->next, sizeof(r->next), "-1");
        memset(r->coord_pending, 0, sizeof(r->coord_pending));

        for (int j = 0; j < MAX_NODES; j++) {
          if (neighbors[j].fd > 0 &&
              strcmp(neighbors[j].id, neighbor_id) != 0) {
            send_coord(node, neighbors[j].id, r->dest);
            r->coord_pending[j] = 1;
          }
        }
        check_coordination_end(node, r);
      } else if (r->state == STATE_COORDINATION) {
        int slot = find_neighbor_slot_by_id(neighbor_id);
        if (slot != -1) {
          r->coord_pending[slot] = 0;
        }
        check_coordination_end(node, r);
      }
    }
  }
}

void process_coord_msg(Node *node, char *j, char *t) {
  Route *r = find_route(node, t);
  if (!r)
    return;
  if (r->state == STATE_COORDINATION) {
    send_uncoord(node, j, t);
  } else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) != 0) {
    send_route_to_id(node, j, t, r->cost);
    send_uncoord(node, j, t);
  } else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) == 0) {
    r->state = STATE_COORDINATION;
    set_text(r->succ_coord, sizeof(r->succ_coord), j);
    r->cost = INF;
    set_text(r->next, sizeof(r->next), "-1");
    memset(r->coord_pending, 0, sizeof(r->coord_pending));

    for (int k = 0; k < MAX_NODES; k++) {
      if (neighbors[k].fd > 0) {
        send_coord(node, neighbors[k].id, t);
        r->coord_pending[k] = 1;
      }
    }
    check_coordination_end(node, r);
  }
}

void send_coord(Node *node, char *target_id, char *dest) {
  char msg[128];
  snprintf(msg, sizeof(msg), "COORD %s\n", dest);
  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd > 0 && strcmp(neighbors[i].id, target_id) == 0) {
      monitor_log("[MONITOR] TX to %s: %s", target_id, msg);
      send_msg(neighbors[i].fd, msg);
    }
  }
  (void)node;
}

void send_uncoord(Node *node, char *target_id, char *dest) {
  char msg[128];
  snprintf(msg, sizeof(msg), "UNCOORD %s\n", dest);
  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd > 0 && strcmp(neighbors[i].id, target_id) == 0) {
      monitor_log("[MONITOR] TX to %s: %s", target_id, msg);
      send_msg(neighbors[i].fd, msg);
    }
  }
  (void)node;
}

void send_route_to_id(Node *node, char *target_id, char *dest, int cost) {
  char msg[128];
  snprintf(msg, sizeof(msg), "ROUTE %s %d\n", dest, cost);

  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd > 0 && strcmp(neighbors[i].id, target_id) == 0) {
      monitor_log("[MONITOR] TX to %s: %s", target_id, msg);
      send_msg(neighbors[i].fd, msg);
      return;
    }
  }
  (void)node;
}

static void check_coordination_end(Node *node, Route *r) {
  if (r->state != STATE_COORDINATION)
    return;

  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd != -1 && r->coord_pending[i]) {
      return;
    }
  }

  r->state = STATE_EXPEDITION;
  if (r->cost < INF) {
    broadcast_routes(node);
  }
  if (strcmp(r->succ_coord, "-1") != 0) {
    send_uncoord(node, r->succ_coord, r->dest);
  }
}

void process_uncoord_msg(Node *node, char *j, char *t) {
  Route *r = find_route(node, t);
  if (!r)
    return;

  int slot = find_neighbor_slot_by_id(j);
  if (slot != -1) {
    r->coord_pending[slot] = 0;
  }
  check_coordination_end(node, r);
}
