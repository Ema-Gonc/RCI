#ifndef COMMON_H
#define COMMON_H

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_NODES 100

typedef struct {
  char id[3];
  int fd;
  char ip[64];
  char tcp[32];
} Neighbor;

typedef struct {
  char dest[64];
  int cost;
  char next[64];
  int state;
  char succ_coord[64];
  int coord_pending[MAX_NODES];
} Route;

typedef struct {
  int route_count;
  Route routes[MAX_NODES];
  int neighbor_count;
  Neighbor neighbors[MAX_NODES];
  char id[3];
} Node;

typedef struct {
  char IP[64];
  char TCP[32];
  char regIP[64];
  char regUDP[32];
  int net; // 000 to 999
  int id;  // 00 to 99
  int monitor; // Flag for routing message monitoring
} AppConfig;

#endif
