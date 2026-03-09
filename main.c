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

// TODO: Streamline user interaction and spacings -> \n

int main(int argc, char *const *argv) {
  /*----------------------
   |  Handle Arguments
   -----------------------*/
  if (argc < 3 || argc > 5) {
    fprintf(
        stderr,
        "Utilização: owr IP TCP [regIP] [regUDP]\n\n"
        "Argumentos obrigatórios:\n"
        "  IP       Endereço IP da máquina que corre a aplicação\n"
        "  TCP      Porto TCP servidor da aplicação\n\n"
        "Argumentos opcionais:\n"
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

  config.net = -1;

  config.id = -1;

  o_init_nb();

  int udp_fd = ns_udp_init(config.regIP, config.regUDP);
  int tcp_listen_fd = o_tcp_listener_init(config.IP, config.TCP);

  printf("Node initialized on %s:%s.\nConnecting to Node Server at %s:%s.\n",
         config.IP, config.TCP, config.regIP, config.regUDP);
  printf("Type commands to start. Try 'join <net> <id>' or 'help'.\n");

  fd_set read_fds;
  int max_fd;

  while (1) {
    FD_ZERO(&read_fds);

    FD_SET(STDIN_FILENO, &read_fds);
    max_fd = STDIN_FILENO;

    if (udp_fd != -1) {
      FD_SET(udp_fd, &read_fds);
      if (udp_fd > max_fd)
        max_fd = udp_fd;
    }

    if (tcp_listen_fd != -1) {
      FD_SET(tcp_listen_fd, &read_fds);
      if (tcp_listen_fd > max_fd)
        max_fd = tcp_listen_fd;
    }

    max_fd = check_nb_max_fd(&read_fds, max_fd);

    int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

    if (activity == -1) {
      perror("Error in select()");
      break;
    }

    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
      char input_buffer[256];
      if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
        ui_process_command(input_buffer, &config, udp_fd);
      }
    }

    if (udp_fd != -1 && FD_ISSET(udp_fd, &read_fds)) {
      ns_handle_response(udp_fd, &config);
    }

    if (tcp_listen_fd != -1 && FD_ISSET(tcp_listen_fd, &read_fds)) {
      o_accept_in(tcp_listen_fd);
    }

    o_read_nb(&read_fds);
  }

  if (udp_fd != -1)
    close(udp_fd);
  if (tcp_listen_fd != -1)
    close(tcp_listen_fd);

  o_rm_all_nb();

  return EXIT_SUCCESS;
}
