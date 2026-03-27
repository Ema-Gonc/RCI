#define _GNU_SOURCE

#include "../include/node_server.h"
#include "../include/common.h"
#include "../include/overlay.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static struct addrinfo *g_server_info = NULL;

#define NS_MAX_PENDING 16
#define NS_TIMEOUT_SEC 3

typedef struct {
  int active;
  int tid;
  char command[16];
  time_t sent_at;
} PendingTx;

static PendingTx g_pending[NS_MAX_PENDING];

static void track_pending_tx(const char *command, int tid) {
  int slot = -1;
  for (int i = 0; i < NS_MAX_PENDING; i++) {
    if (!g_pending[i].active) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    slot = 0;
  }

  g_pending[slot].active = 1;
  g_pending[slot].tid = tid;
  snprintf(g_pending[slot].command, sizeof(g_pending[slot].command), "%s",
           command);
  g_pending[slot].sent_at = time(NULL);
}

static void clear_pending_tx(int tid) {
  for (int i = 0; i < NS_MAX_PENDING; i++) {
    if (g_pending[i].active && g_pending[i].tid == tid) {
      g_pending[i].active = 0;
      return;
    }
  }
}

void ns_tick(void) {
  time_t now = time(NULL);
  for (int i = 0; i < NS_MAX_PENDING; i++) {
    if (!g_pending[i].active) {
      continue;
    }

    if ((now - g_pending[i].sent_at) >= NS_TIMEOUT_SEC) {
      printf("Timeout waiting Node Server response for %s (Transaction: %03d)\n",
             g_pending[i].command, g_pending[i].tid);
      g_pending[i].active = 0;
    }
  }
}

int ns_udp_init(const char *regIP, const char *regUDP) {
  struct addrinfo hints;
  int udp_fd;
  int errcode;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  errcode = getaddrinfo(regIP, regUDP, &hints, &g_server_info);
  if (errcode != 0) {
    fprintf(stderr, "Error resolving Node Server address: %s\n",
            gai_strerror(errcode));
    return -1;
  }

  udp_fd = socket(g_server_info->ai_family, g_server_info->ai_socktype,
                  g_server_info->ai_protocol);
  if (udp_fd == -1) {
    perror("Error creating UDP socket");
    freeaddrinfo(g_server_info);
    g_server_info = NULL;
    return -1;
  }

  return udp_fd;
}

// REG tid[3] op[1] net[3] id[2] IP TCP
int ns_send_reg(int udp_fd, int op, int net, int id, const char *node_ip,
                const char *node_tcp) {
  char buffer[128];
  int tid = rand() % 1000; // random tid

  if (g_server_info == NULL) {
    fprintf(stderr,
            "Node Server address not initialized. Call ns_udp_init first.\n");
    return -1;
  }

  if (op == 0) // Registration
  {
    sprintf(buffer, "REG %03d 0 %03d %02d %s %s\n", tid, net, id, node_ip,
            node_tcp);
  } else if (op == 3) // Unregistration
  {
    sprintf(buffer, "REG %03d 3 %03d %02d\n", tid, net, id);
  } else // Invalid operation
  {
    return -1;
  }

  ssize_t n = sendto(udp_fd, buffer, strlen(buffer), 0, g_server_info->ai_addr,
                     g_server_info->ai_addrlen);
  if (n == -1) {
    perror("Error sending REG message");
    return -1;
  }

  track_pending_tx("REG", tid);

  return tid;
}

// NODES tid[3] op[1] net[3]
int ns_send_nodes(int udp_fd, int net) {
  char buffer[128];
  int tid = rand() % 1000; // random tid

  if (g_server_info == NULL) {
    fprintf(stderr,
            "Node Server address not initialized. Call ns_udp_init first.\n");
    return -1;
  }

  sprintf(buffer, "NODES %03d 0 %03d", tid, net);

  ssize_t n = sendto(udp_fd, buffer, strlen(buffer), 0, g_server_info->ai_addr,
                     g_server_info->ai_addrlen);

  if (n == -1) {
    perror("Error sending NODES message");
    return -1;
  }

  track_pending_tx("NODES", tid);

  return tid;
}

// CONTACT tid[3] op[1] net[3] id[2]
int ns_send_contact(int udp_fd, int net, int target_id) {
  char buffer[128];
  int tid = rand() % 1000; // random tid

  if (g_server_info == NULL) {
    fprintf(stderr,
            "Node Server address not initialized. Call ns_udp_init first.\n");
    return -1;
  }

  sprintf(buffer, "CONTACT %03d 0 %03d %02d\n", tid, net, target_id);
  if (sendto(udp_fd, buffer, strlen(buffer), 0, g_server_info->ai_addr,
             g_server_info->ai_addrlen) == -1) {
    perror("Error sending CONTACT message");
    return -1;
  }

  track_pending_tx("CONTACT", tid);

  return tid;
}

void ns_handle_response(int udp_fd, const AppConfig *config) {
  char buffer[1024];
  struct sockaddr_storage from_addr;
  socklen_t from_len = sizeof(from_addr);

  ssize_t n = recvfrom(udp_fd, buffer, sizeof(buffer) - 1, 0,
                       (struct sockaddr *)&from_addr, &from_len);
  if (n == -1) {
    perror("Error receiving from Node Server");
    return;
  }

  buffer[n] = '\0';

  if (config && config->monitor) {
    monitor_log("[MONITOR] RX NS: %s\n", buffer);
  }

  char command[9]; // Longest expected command is "CONTACT".
  int tid, op;

  if (sscanf(buffer, "%8s %d %d", command, &tid, &op) >= 3) {
    clear_pending_tx(tid);

    // Handle REG responses
    if (strcmp(command, "REG") == 0) {
      if (op == 1) {
        printf("Success: Node successfully registered! (Transaction: %03d)\n",
               tid);
      } else if (op == 2) {
        printf("Error: Node Server database is full! (Transaction: %03d)\n",
               tid);
      } else if (op == 4) {
        printf("Success: Node successfully unregistered! (Transaction: %03d)\n",
               tid);
      } else if (op == 7) {
        printf("Error: ID already in use! (Transaction: %03d)\n", tid);
      } else {
        printf("Unknown REG operation received: %d\n", op);
      }
    }

    // Handle NODES responses
    else if (strcmp(command, "NODES") == 0) {
      if (op == 1) {
        int net;
        sscanf(buffer, "%*s %*d %*d %d", &net);
        printf("Nodes in network %03d:\n", net);

        strtok(buffer, "\n");            // skip first line
        char *line = strtok(NULL, "\n"); // move to next line

        int count = 0;
        while (line != NULL) {
          if (strlen(line) > 0) {
            printf(" - Node ID: %s\n", line);
            count++;
          }
          line = strtok(NULL, "\n"); // move to next line
        }

        if (count == 0) {
          printf(" (No nodes are currently registered in this network)\n");
        }
      } else {
        printf("Unknown NODES operation received: %d\n", op);
      }
    }

    // Handle CONTACT responses
    else if (strcmp(command, "CONTACT") == 0) {
      if (op == 1) {
        int net, target_id;
        char target_ip[64], target_tcp[32];

        // CONTACT NET[3] OP[1] NET[3] ID[2] IP TCP
        sscanf(buffer, "%*s %*d %*d %d %d %63s %31s", &net, &target_id,
               target_ip, target_tcp);

        printf("Connecting to Node %02d at %s:%s...\n", target_id, target_ip,
               target_tcp);
        if (config == NULL || config->id < 0) {
          printf("Error: local node id is not set. Join a network before "
                 "adding edges.\n");
          return;
        }

        if (o_connect_out(target_ip, target_tcp, target_id, config->id) != 0)
          printf("CONTACT connect failed for node %02d.\n", target_id);

      } else if (op == 2) {
        printf("Error: Node is not registered on the Node Server.\n");
      }
    } else {
      printf("Unknown command received: %s\n", command);
    }
  } else {
    printf("Error: Malformed message from Node Server.\n");
  }
}
