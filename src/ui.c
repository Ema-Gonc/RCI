

/*
 * Módulo de Interface de Utilizador (UI):
 * - Processamento e parsing de comandos interativos via terminal (STDIN).
 * - Gestão de estados de adesão (Join/Leave/Exit) e registo no servidor central.
 * - Comandos de exploração de rede: listagem de nós ativos e detalhes de vizinhança.
 * - Manipulação de topologia (Edges): adição e remoção de ligações TCP entre nós.
 * - Suporte a operações diretas: configuração de ID e conexões bypass ao servidor.
 * - Sistema de ajuda e validação de argumentos para interação robusta com o utilizador.
 */


#include "../include/ui.h"
#include "../include/common.h"
#include "../include/node_server.h"
#include "../include/overlay.h"
#include "../include/routing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "routing.h" // Uncomment when routing is implemented
// #include "chat.h"    // Uncomment when chat is implemented

extern Neighbor neighbors[MAX_NODES];

extern Node my_node;

static void ui_leave_network(AppConfig *config, int udp_fd, int verbose);

static void ui_print_help(void) {
  printf("Available commands:\n");
  printf("  help | h\n");
  printf("  join | j <net> <id>\n");
  printf("  show nodes | n [<net>]\n");
  printf("  leave | l\n");
  printf("  exit | x\n");
  printf("  add edge | ae <id>\n");
  printf("  remove edge | re <id>\n");
  printf("  show neighbors | sg\n");
  printf("  announce | a\n");
  printf("  show routing | sr <dest>\n");
  printf("  message | m <dest> <message text>\n");
  printf("  start monitor | sm\n");
  printf("  end monitor | em\n");
  printf("  direct join | dj <net> <id>\n");
  printf("  direct add edge | dae <id> <IP> <TCP>\n");
}

void ui_process_command(char *input, AppConfig *config, int udp_fd) {
  char command[32];
  int net, id;
  char ip[64], tcp[32];

  if (sscanf(input, "%31s", command) <= 0)
    return;

  // 0. HELP: help (h)
  if (strcmp(command, "help") == 0 || strcmp(command, "h") == 0) {
    ui_print_help();
    return;
  }

  // 1. JOIN: join (j) net id
  else if (strcmp(command, "join") == 0 || strcmp(command, "j") == 0) {
    if (sscanf(input, "%*s %d %d", &net, &id) == 2) {
      config->net = net;
      config->id = id;
      printf("Joining network %03d as node %02d...\n", net, id);
      ns_send_reg(udp_fd, 0, net, id, config->IP, config->TCP);
      // Initialize routing
      char my_id_str[3];
      sprintf(my_id_str, "%02d", id);
      node_init(&my_node, my_id_str, "", 0);
      broadcast_routes(&my_node);
    } else {
      printf("Usage: join <net> <id>\n");
    }
  }

  // 2. SHOW NODES: show nodes (n) net
  else if (strcmp(command, "n") == 0 ||
           (strncmp(input, "show nodes", 10) == 0)) {
    int has_net = (sscanf(input, "%*s %d", &net) == 1 ||
                   sscanf(input, "show nodes %d", &net) == 1);

    if (!has_net) {
      if (config->net == -1) {
        printf("Error: no default network. Join a network or use 'show nodes "
               "<net>'.\n");
        return;
      }
      net = config->net;
    }

    printf("Requesting list of nodes for network %03d...\n", net);
    ns_send_nodes(udp_fd, net);
  }

  // 3. LEAVE: leave (l)
  else if (strcmp(command, "leave") == 0 || strcmp(command, "l") == 0) {
    ui_leave_network(config, udp_fd, 1);
  }

  // 4. EXIT: exit (x)
  else if (strcmp(command, "exit") == 0 || strcmp(command, "x") == 0) {
    // Spec behavior: if still joined, perform leave first.
    ui_leave_network(config, udp_fd, 0);
    printf("Exiting OWR Application. Goodbye!\n");
    exit(0);
  }

  // 5. ADD EDGE: add edge (ae) id
  else if (strcmp(command, "ae") == 0 || (strncmp(input, "add edge", 8) == 0)) {
    if (config->net == -1) {
      printf("Error: You must join a network first.\n");
      return;
    }
    if (sscanf(input, "%*s %d", &id) == 1 ||
        sscanf(input, "add edge %d", &id) == 1) {
      printf("Requesting contact info for node %02d...\n", id);
      ns_send_contact(udp_fd, config->net, id);
    } else {
      printf("Usage: add edge <id>\n");
    }
  }

  // 6. REMOVE EDGE: remove edge (re) id
  else if (strcmp(command, "re") == 0 ||
           (strncmp(input, "remove edge", 11) == 0)) {
    if (sscanf(input, "%*s %d", &id) == 1 ||
        sscanf(input, "remove edge %d", &id) == 1) {
      if (config->id >= 0 && id == config->id) {
        printf("Error: cannot remove an edge to yourself (node %02d).\n", id);
        return;
      }

      printf("Removing edge with node %02d.\n", id);
      o_rm_nb(id);
    } else {
      printf("Usage: remove edge <id>\n");
    }
  }

  // 7. SHOW NEIGHBORS: show neighbors (sn)
  else if (strcmp(command, "sg") == 0 ||
           (strncmp(input, "show neighbors", 14) == 0)) {
    printf("Active Neighbors:\n");
    int count = 0;
    for (int i = 0; i < MAX_NODES; i++) {
      if (neighbors[i].fd != -1) {
        printf(" - Node %s (FD: %d)\n", neighbors[i].id, neighbors[i].fd);
        count++;
      }
    }
    if (count == 0)
      printf(" (No active edges)\n");
  }

  // 8. DIRECT JOIN: direct join (dj) net id
  else if (strcmp(command, "dj") == 0 ||
           (strncmp(input, "direct join", 11) == 0)) {
    if (sscanf(input, "%*s %d %d", &net, &id) == 2 ||
        sscanf(input, "direct join %d %d", &net, &id) == 2) {
      config->net = net;
      config->id = id;
      printf("Directly joined network %03d as node %02d (No Node Server "
             "registration).\n",
             net, id);
      // Initialize routing
      char my_id_str[3];
      sprintf(my_id_str, "%02d", id);
      node_init(&my_node, my_id_str, "", 0);
      broadcast_routes(&my_node);
    } else {
      printf("Usage: direct join <net> <id>\n");
    }
  }

  // 9. DIRECT ADD EDGE: direct add edge (dae) id IP TCP
  else if (strcmp(command, "dae") == 0 ||
           (strncmp(input, "direct add edge", 15) == 0)) {
    if (config->id == -1) {
      printf("Error: You must join or direct join a network first.\n");
      return;
    }
    if (sscanf(input, "%*s %d %63s %31s", &id, ip, tcp) == 3 ||
        sscanf(input, "direct add edge %d %63s %31s", &id, ip, tcp) == 3) {
      printf("Directly connecting to node %02d at %s:%s...\n", id, ip, tcp);
      if (o_connect_out(ip, tcp, id, config->id) != 0) {
        printf("Failed to establish direct edge to node %02d.\n", id);
      }
    } else {
      printf("Usage: direct add edge <id> <IP> <TCP>\n");
    }
  }

  // 10. ANNOUNCE: announce (a)
  else if (strcmp(command, "a") == 0 || strncmp(input, "announce", 8) == 0) {
    if (config->net == -1) {
      printf("Error: You must join a network first.\n");
      return;
    }
    broadcast_routes(&my_node);
    printf("Routes announced to all neighbors.\n");
  }

  // 11. SHOW ROUTING: show routing (sr) dest
  else if (strcmp(command, "sr") == 0 || strncmp(input, "show routing", 12) == 0) {
    char dest[64];
    if (sscanf(input, "%*s %63s", dest) == 1 ||
        sscanf(input, "show routing %63s", dest) == 1) {
      Route *r = find_route(&my_node, dest);
      if (r) {
        printf("Route to %s: Next hop %s, Cost %d, State %s\n",
               dest, r->next, r->cost,
               (r->state == 0) ? "EXPEDITION" : "COORDINATION");
      } else {
        printf("No route to %s\n", dest);
      }
    } else {
      printf("Usage: show routing <dest>\n");
    }
  }

  // 12. MESSAGE: message (m) dest text
  else if (strcmp(command, "m") == 0 || strncmp(input, "message", 7) == 0) {
    char dest[64], msg[256];
    if (sscanf(input, "%*s %63s %255[^\n]", dest, msg) == 2 ||
        sscanf(input, "message %63s %255[^\n]", dest, msg) == 2) {
      // Find next hop
      char *next_hop = get_next_hop(&my_node, dest);
      if (next_hop) {
        // Find socket for next_hop
        int sock = -1;
        for (int i = 0; i < MAX_NODES; i++) {
          if (neighbors[i].fd != -1 && strcmp(neighbors[i].id, next_hop) == 0) {
            sock = neighbors[i].fd;
            break;
          }
        }
        if (sock != -1) {
          send_chat(sock, dest, msg);
          printf("Message sent to %s via %s\n", dest, next_hop);
        } else {
          printf("No active connection to next hop %s\n", next_hop);
        }
      } else {
        printf("No route to %s\n", dest);
      }
    } else {
      printf("Usage: message <dest> <text>\n");
    }
  }

  // 13. START MONITOR: start monitor (sm)
  else if (strcmp(command, "sm") == 0 || strncmp(input, "start monitor", 13) == 0) {
    config->monitor = 1;
    printf("Routing message monitoring ON.\n");
    printf("Monitoring ON: logs at /tmp/owr_monitor.log\n");
    FILE *f = fopen("/tmp/owr_monitor.log", "a");
    if (f) {
      fprintf(f, "=== START MONITORING (node %02d) ===\n", config->id);
      fclose(f);
    }
  }

  // 14. END MONITOR: end monitor (em)
  else if (strcmp(command, "em") == 0 || strncmp(input, "end monitor", 11) == 0) {
    config->monitor = 0;
    printf("Monitoring OFF.\n");
    FILE *f = fopen("/tmp/owr_monitor.log", "a");
    if (f) {
      fprintf(f, "=== END MONITORING (node %02d) ===\n", config->id);
      fclose(f);
    }
  }

  // UNKNOWN COMMAND
  else {
    printf("Unknown command: %s\n", command);
  }
}

static void ui_leave_network(AppConfig *config, int udp_fd, int verbose) {
  if (config->net == -1 || config->id == -1) {
    if (verbose) {
      printf("You are not currently in a network.\n");
    }
    return;
  }

  if (verbose) {
    printf("Leaving network %03d...\n", config->net);
  }

  // Send unregister message (op = 3), IP and TCP are omitted
  ns_send_reg(udp_fd, 3, config->net, config->id, "", "");

  // Remove all overlay edges before exiting/leaving.
  o_rm_all_nb();

  config->net = -1;
  config->id = -1;

  // Clear routing state.
  my_node.route_count = 0;
  my_node.neighbor_count = 0;

  if (verbose) {
    printf("Successfully left the network and closed all edges.\n");
  }
}