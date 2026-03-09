#ifndef ROUTING_H
#define ROUTING_H

#include "node.h"

#define STATE_EXPEDITION 0 
#define STATE_COORDINATION 1 

void process_route_update(Node *node, char *neighbor, char *dest, int cost); 

void broadcast_routes(Node *node);
void send_routes(Node *node); 

void handle_edge_failure(Node *node, char *neighbor_id); 
void process_coord_msg(Node *node, char *neighbor, char *dest); 
void process_uncoord_msg(Node *node, char *neighbor, char *dest); 
void check_coordination_end(Node *node, Route *r); 
void send_coord(Node *node, char *target_id, char *dest); 
void send_uncoord(Node *node, char *target_id, char *dest);
void send_route_to_id(Node *node, char *target_id, char *dest, int cost); 

#endif