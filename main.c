/*
 * Módulo Principal e Ciclo de Eventos (Main Loop):
 * - Ponto de entrada da aplicação OWR, responsável pelo parsing de argumentos e setup inicial.
 * - Orquestração de sockets UDP (Servidor) e TCP (Escuta/Overlay) para operação híbrida.
 * - Implementação de I/O Multiplexing via 'select' para gestão concorrente de eventos.
 * - Encaminhamento de mensagens para os sub-módulos: UI, Node Server e Overlay.
 * - Gestão do estado global da aplicação e encerramento controlado de recursos de rede.
 */

/*----------------------
 | Class: RCI
 | Project: OverlayWithRouting (OWR)
 | Author: André Santos & Ema Gonçalves
 ----------------------*/

/*----------------------
|  Includes & Defs
-----------------------*/

#include "include/common.h"
#include "include/node_server.h"
#include "include/overlay.h"
#include "include/ui.h"
#include "include/routing.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_RIP "193.136.138.142"
#define DEFAULT_PORT 59000

AppConfig config;
Node my_node;

// TODO: Streamline user interaction and spacings -> \n

static int parse_arguments(int argc, char *const *argv) {
  if (argc < 3 || argc > 5) {
    fprintf(
        stderr,
        "Utilização: owr IP TCP [regIP] [regUDP]\n\n"
        "Argumentos obrigatórios:\n"
        "  IP       Endereço IP da máquina que corre a aplicação\n"
        "  TCP      Porto TCP servidor da aplicação\n\n"
        "  Argumentos opcionais:\n"
        "  regIP    Endereço IP do servidor de nós (default: 193.136.138.142)\n"
        "  regUDP   Porto UDP do servidor de nós (default: 59000)\n");
    return EXIT_FAILURE;
  }

  strncpy(config.IP, argv[1], sizeof(config.IP) - 1);
  config.IP[sizeof(config.IP) - 1] = '\0';
  strncpy(config.TCP, argv[2], sizeof(config.TCP) - 1);
  config.TCP[sizeof(config.TCP) - 1] = '\0';

  if (argc >= 4) {
    strncpy(config.regIP, argv[3], sizeof(config.regIP) - 1);
    config.regIP[sizeof(config.regIP) - 1] = '\0';
  } else {
    strncpy(config.regIP, "193.136.138.142", sizeof(config.regIP) - 1);
    config.regIP[sizeof(config.regIP) - 1] = '\0';
  }

  if (argc == 5) {
    strncpy(config.regUDP, argv[4], sizeof(config.regUDP) - 1);
    config.regUDP[sizeof(config.regUDP) - 1] = '\0';
  } else {
    strncpy(config.regUDP, "59000", sizeof(config.regUDP) - 1);
    config.regUDP[sizeof(config.regUDP) - 1] = '\0';
  }

  return EXIT_SUCCESS;
}

static void initialize_config(void) {
  config.net = -1;
  config.id = -1;
  config.monitor = 0;
  o_init_nb();
}

static void setup_fds(fd_set *read_fds, int udp_fd, int tcp_listen_fd, int *max_fd) {
  FD_ZERO(read_fds);
  FD_SET(STDIN_FILENO, read_fds);
  *max_fd = STDIN_FILENO;

  if (udp_fd != -1) {
    FD_SET(udp_fd, read_fds);
    if (udp_fd > *max_fd)
      *max_fd = udp_fd;
  }

  if (tcp_listen_fd != -1) {
    FD_SET(tcp_listen_fd, read_fds);
    if (tcp_listen_fd > *max_fd)
      *max_fd = tcp_listen_fd;
  }

  *max_fd = check_nb_max_fd(read_fds, *max_fd);
}

static void handle_events(fd_set *read_fds, int udp_fd, int tcp_listen_fd) {
  if (FD_ISSET(STDIN_FILENO, read_fds)) {
    char input_buffer[256];
    if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
      ui_process_command(input_buffer, &config, udp_fd);
    }
  }

  if (udp_fd != -1 && FD_ISSET(udp_fd, read_fds)) {
    ns_handle_response(udp_fd, &config);
  }

  if (tcp_listen_fd != -1 && FD_ISSET(tcp_listen_fd, read_fds)) {
    o_accept_in(tcp_listen_fd);
  }

  o_read_nb(read_fds);
}

static void cleanup(int udp_fd, int tcp_listen_fd) {
  if (udp_fd != -1)
    close(udp_fd);
  if (tcp_listen_fd != -1)
    close(tcp_listen_fd);

  o_rm_all_nb();
}

int main(int argc, char *const *argv) {
  if (parse_arguments(argc, argv) == EXIT_FAILURE)
    return EXIT_FAILURE;

  initialize_config();

  int udp_fd = ns_udp_init(config.regIP, config.regUDP);
  int tcp_listen_fd = o_tcp_listener_init(config.IP, config.TCP);

  printf("Node initialized at %s:%s.\n", config.IP, config.TCP);

  fd_set read_fds;
  int max_fd;

  while (1) {
    setup_fds(&read_fds, udp_fd, tcp_listen_fd, &max_fd);

    int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

    if (activity == -1) {
      perror("Error in select()");
      break;
    }

    handle_events(&read_fds, udp_fd, tcp_listen_fd);
  }

  cleanup(udp_fd, tcp_listen_fd);

  return EXIT_SUCCESS;
}
