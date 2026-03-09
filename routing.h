#ifndef ROUTING_H
#define ROUTING_H

#include "node.h"


void process_route(Node *node, char *neighbor, char *dest, int cost);
void broadcast_routes(Node *node);
void send_routes(Node *node);
void process_route_update(Node *node, char *neighbor, char *dest, int cost);

#endif