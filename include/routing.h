#ifndef ROUTING_H
#define ROUTING_H

#include "common.h"

void process_route(Node *node, char *neighbor_id, char *dest, int cost);
void broadcast_routes(Node *node);
void send_routes(Node *node);

void process_coord_msg(Node *node, char *neighbor_id, char *dest);
void process_uncoord_msg(Node *node, char *neighbor_id, char *dest);

void handle_edge_failure(Node *node, char *neighbor_id);

void send_coord(Node *node, char *target_id, char *dest);
void send_uncoord(Node *node, char *target_id, char *dest);
void send_route_to_id(Node *node, char *target_id, char *dest, int cost);

// Additional functions
void node_init(Node *node, char *id, const char *ip, int port);
void add_neighbor(Node *node, char *id, char *ip, int port);
Route *find_route(Node *node, char *dest);
int add_route(Node *node, char *dest, char *next, int cost);
char *get_succ(Node *node, char *dest);
void print_routes(Node *node);
void send_chat(int sock, char *dest, char *msg);
void send_route_update(int sock, char *dest, int cost);

#endif // ROUTING_H
