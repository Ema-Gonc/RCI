
/*
 * Módulo de Gestão da Camada Overlay (TCP):
 * - Inicialização de servidor TCP e estabelecimento de conexões ativas com pares.
 * - Implementação do handshake "NEIGHBOR" para troca de identidades entre nós.
 * - Gestão dinâmica da tabela de vizinhos (ID, IP, Porto e File Descriptors).
 * - Tratamento de eventos de rede: aceitação, leitura de dados e fecho de sockets.
 * - Integração com o mecanismo de multiplexagem (select) para monitorização de múltiplos FDs.
 */



#define _GNU_SOURCE

#include "../include/overlay.h"
#include "../include/common.h"
#include "../include/routing.h"

#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Neighbor neighbors[MAX_NODES];

// External config for monitoring
extern AppConfig config;

// Helper function to log messages if monitoring enabled
static void monitor_log(const char *format, ...) {
    if (config.monitor) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

static char g_local_ip[64] = {0};
static char g_local_tcp[32] = {0};

extern Node my_node;

static int find_free_slot(void);
static int find_slot_by_id(int id);
static void clear_slot(int slot);

int o_tcp_listener_init(const char *ip, const char *port) {
  struct addrinfo hints, *res;
  int listen_fd, errcode;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (ip != NULL) {
    strncpy(g_local_ip, ip, sizeof(g_local_ip) - 1);
    g_local_ip[sizeof(g_local_ip) - 1] = '\0';
  }
  if (port != NULL) {
    strncpy(g_local_tcp, port, sizeof(g_local_tcp) - 1);
    g_local_tcp[sizeof(g_local_tcp) - 1] = '\0';
  }

  (void)ip;

  errcode = getaddrinfo(NULL, port, &hints, &res);
  if (errcode != 0) {
    printf("Error resolving local listen address.\n");
    exit(1);
  }

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    perror("Error creating listening socket");
    exit(1);
  }

  if (bind(listen_fd, res->ai_addr, res->ai_addrlen) == -1) {
    perror("Error binding listening socket");
    exit(1);
  }

  freeaddrinfo(res);

  if (listen(listen_fd, 5) == -1) {
    perror("Error listening on socket");
    exit(1);
  }

  return listen_fd;
}

void o_accept_in(int listen_fd) {
  struct sockaddr_storage incoming_addr;
  socklen_t addrlen = sizeof(incoming_addr);
  int new_edge_fd;

  new_edge_fd = accept(listen_fd, (struct sockaddr *)&incoming_addr, &addrlen);

  if (new_edge_fd == -1) {
    perror("Error accepting new connection");
    return;
  }

  // printf("New TCP connection accepted! (FD: %d)\n", new_edge_fd);

  int slot = find_free_slot();
  if (slot == -1) {
    fprintf(
        stderr,
        "No free neighbor slots available. Closing new connection (FD: %d).\n",
        new_edge_fd);
    close(new_edge_fd);
    return;
  }

  neighbors[slot].fd = new_edge_fd;
  strcpy(neighbors[slot].id, "-1");
  memset(neighbors[slot].ip, 0, sizeof(neighbors[slot].ip));
  memset(neighbors[slot].tcp, 0, sizeof(neighbors[slot].tcp));
}

int o_connect_out(const char *target_ip, const char *target_port, int target_id,
                  int my_id) {
  struct addrinfo hints, *res;
  int edge_fd, errcode;

  if (target_ip != NULL && target_port != NULL && g_local_ip[0] != '\0' &&
      g_local_tcp[0] != '\0') {
    if (strcmp(target_ip, g_local_ip) == 0 &&
        strcmp(target_port, g_local_tcp) == 0) {
      printf("Error: refusing to connect to self (%s:%s).\n", target_ip,
             target_port);
      return -1;
    }
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  errcode = getaddrinfo(target_ip, target_port, &hints, &res);
  if (errcode != 0) {
    printf("Error resolving peer address.\n");
    return -1;
  }

  edge_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (edge_fd == -1) {
    perror("Error creating TCP socket");
    freeaddrinfo(res);
    return -1;
  }

  if (connect(edge_fd, res->ai_addr, res->ai_addrlen) == -1) {
    perror("Error connecting to peer");
    close(edge_fd);
    freeaddrinfo(res);
    return -1;
  }

  freeaddrinfo(res);

  char msg[64];
  snprintf(msg, sizeof(msg), "NEIGHBOR %02d\n", my_id);

  ssize_t nleft = strlen(msg);
  char *ptr = msg;
  while (nleft > 0) {
    ssize_t nwritten = write(edge_fd, ptr, nleft);
    if (nwritten <= 0) {
      perror("Error sending NEIGHBOR message");
      close(edge_fd);
      return -1;
    }
    nleft -= nwritten;
    ptr += nwritten;
  }

  // printf("Successfully connected to peer and sent NEIGHBOR message.\n");

  o_add_nb(target_id, edge_fd, target_ip, target_port);

  return 0;
}

void o_init_nb(void) {
  for (int i = 0; i < MAX_NODES; i++) {
    strcpy(neighbors[i].id, "-1");
    neighbors[i].fd = -1;
    memset(neighbors[i].ip, 0, sizeof(neighbors[i].ip));
    memset(neighbors[i].tcp, 0, sizeof(neighbors[i].tcp));
  }
}

void o_add_nb(int id, int fd, const char *ip, const char *tcp) {
  if (id < 0)
    return;

  int slot = find_slot_by_id(id);
  if (slot == -1)
    slot = find_free_slot();

  if (slot == -1) {
    fprintf(stderr, "No free neighbor slots available.\n");
    if (fd != -1)
      close(fd);
    return;
  }

  if (neighbors[slot].fd != -1 && neighbors[slot].fd != fd)
    close(neighbors[slot].fd);

  sprintf(neighbors[slot].id, "%02d", id);
  neighbors[slot].fd = fd;

  if (ip) {
    strncpy(neighbors[slot].ip, ip, sizeof(neighbors[slot].ip) - 1);
    neighbors[slot].ip[sizeof(neighbors[slot].ip) - 1] = '\0';
  }
  if (tcp) {
    strncpy(neighbors[slot].tcp, tcp, sizeof(neighbors[slot].tcp) - 1);
    neighbors[slot].tcp[sizeof(neighbors[slot].tcp) - 1] = '\0';
  }

  // Automatically announce current routing table after establishing an edge.
  // This ensures any new neighbor receives the routing state immediately.
  broadcast_routes(&my_node);
}

void o_read_nb(fd_set *read_fds) {
  for (int i = 0; i < MAX_NODES; i++) {
    int fd = neighbors[i].fd;

    if (fd == -1 || !FD_ISSET(fd, read_fds))
      continue;

    char buffer[256];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);

    if (n == -1) {
      perror("Error reading from neighbor socket");
      clear_slot(i);
    } else if (n == 0) {
      printf("Neighbor %s disconnected. Closing edge (FD: %d)\n",
             neighbors[i].id, fd);
      clear_slot(i);
    } else {
      buffer[n] = '\0';

      char command[32];
      int id;

      if (sscanf(buffer, "%31s", command) >= 1) {
        // NEIGHBOUR id[2]
        if (strcmp(command, "NEIGHBOR") == 0) {
          if (sscanf(buffer, "%*s %d", &id) == 1) {
            int existing_slot = find_slot_by_id(id);
            if (existing_slot == -1 || existing_slot == i) {
              sprintf(neighbors[i].id, "%02d", id);
            } else {
              // Duplicate edge to same neighbor: keep current and close redundant one
              close(neighbors[existing_slot].fd);
              neighbors[existing_slot].fd = -1;
              strcpy(neighbors[existing_slot].id, "-1");
              sprintf(neighbors[i].id, "%02d", id);
            }
            // printf("Successfully established edge with Node %02d!\n", id);
          }
        } else if (strcmp(command, "ROUTE") == 0) {
          char dest[64];
          int cost;
          if (sscanf(buffer, "%*s %63s %d", dest, &cost) == 2) {
            monitor_log("[MONITOR] Overlay received ROUTE %s cost=%d from %s\n", dest, cost, neighbors[i].id);
            process_route(&my_node, neighbors[i].id, dest, cost);
          }
        } else if (strcmp(command, "COORD") == 0) {
          char dest[64];
          if (sscanf(buffer, "%*s %63s", dest) == 1) {
            monitor_log("[MONITOR] Overlay received COORD %s from %s\n", dest, neighbors[i].id);
            process_coord_msg(&my_node, neighbors[i].id, dest);
          }
        } else if (strcmp(command, "UNCOORD") == 0) {
          char dest[64];
          if (sscanf(buffer, "%*s %63s", dest) == 1) {
            monitor_log("[MONITOR] Overlay received UNCOORD %s from %s\n", dest, neighbors[i].id);
            process_uncoord_msg(&my_node, neighbors[i].id, dest);
          }
        } else if (strcmp(command, "CHAT") == 0) {
          char dest[64], msg[256];
          if (sscanf(buffer, "%*s %63s %255[^\n]", dest, msg) == 2) {
            monitor_log("[MONITOR] Overlay received CHAT to %s: %s\n", dest, msg);
            if (strcmp(dest, my_node.id) == 0) {
              printf("Chat received: %s\n", msg);
            } else {
              // Forward
              char *next_hop = get_next_hop(&my_node, dest);
              if (next_hop) {
                int sock = -1;
                for (int j = 0; j < MAX_NODES; j++) {
                  if (neighbors[j].fd != -1 && strcmp(neighbors[j].id, next_hop) == 0) {
                    sock = neighbors[j].fd;
                    break;
                  }
                }
                if (sock != -1) {
                  send_chat(sock, dest, msg);
                }
              }
            }
          }
        } else {
          // printf("Received unknown message from neighbor: %s\n", buffer);
        }
      }
    }
  }
}

void o_rm_nb(int id) {
  if (id < 0)
    return;

  int slot = find_slot_by_id(id);
  if (slot == -1)
    return;

  clear_slot(slot);
}

void o_rm_all_nb(void) {
  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd != -1) {
      clear_slot(i);
    }
  }
}

int check_nb_max_fd(fd_set *read_fds, int max_fd) {
  for (int i = 0; i < MAX_NODES; i++) {
    int neighbor_fd = neighbors[i].fd;
    if (neighbor_fd != -1) {
      FD_SET(neighbor_fd, read_fds);
      if (neighbor_fd > max_fd)
        max_fd = neighbor_fd;
    }
  }

  return max_fd;
}

static int find_free_slot(void) {
  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd == -1) {
      return i;
    }
  }
  return -1;
}

static int find_slot_by_id(int id) {
  char id_str[3];
  sprintf(id_str, "%02d", id);
  for (int i = 0; i < MAX_NODES; i++) {
    if (neighbors[i].fd != -1 && strcmp(neighbors[i].id, id_str) == 0) {
      return i;
    }
  }
  return -1;
}

static void clear_slot(int slot) {
  if (slot < 0 || slot >= MAX_NODES) {
    return;
  }

  if (neighbors[slot].fd != -1) {
    // Handle edge failure for routing
    if (my_node.route_count > 0) {
      handle_edge_failure(&my_node, neighbors[slot].id);
    }
    close(neighbors[slot].fd);
    // printf("Closed connection with Node %s (FD: %d)\n", neighbors[slot].id,
    //        neighbors[slot].fd);
  }

  strcpy(neighbors[slot].id, "-1");
  neighbors[slot].fd = -1;
  memset(neighbors[slot].ip, 0, sizeof(neighbors[slot].ip));
  memset(neighbors[slot].tcp, 0, sizeof(neighbors[slot].tcp));
}



//
