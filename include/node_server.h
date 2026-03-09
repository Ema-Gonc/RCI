#ifndef NODE_SERVER_H
#define NODE_SERVER_H

#include "common.h"
#include <netdb.h>

int ns_udp_init(const char *regIP, const char *regUDP);

int ns_send_reg(int udp_fd, int op, int net, int id, const char *node_ip, const char *node_tcp);
int ns_send_nodes(int udp_fd, int net);
int ns_send_contact(int udp_fd, int net, int target_id);

void ns_handle_response(int udp_fd, const AppConfig *config);

#endif