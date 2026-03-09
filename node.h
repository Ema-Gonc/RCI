#ifndef NODE_H
#define NODE_H

#define MAX_NEIGHBORS 20
#define MAX_ROUTES 100
#define MAX_ID 32
#define BUFFER_SIZE 512
#define INF 999

typedef struct {
    char id[MAX_ID];
    char ip[64];
    int port;
    int socket;
} Neighbor;

typedef struct {
    char dest[MAX_ID];
    char next[MAX_ID];
    int cost;
    int state;             // 0: EXPED, 1: COORD
    char succ_coord[MAX_ID]; 
    int coord_pending[MAX_NEIGHBORS]; 
} Route;

typedef struct {
    char id[MAX_ID];
    char ip[64];           
    int port;              
    int net;               
    int server_socket;

    Neighbor neighbors[MAX_NEIGHBORS];
    int neighbor_count;

    Route routes[MAX_ROUTES];
    int route_count;
} Node;

void node_init(Node *n, char *id, char *ip, int port);
void add_neighbor(Node *n,char *id,char *ip,int port);
void add_route(Node *n,char *dest,char *next,int cost);
char* get_next_hop(Node *n,char *dest);
void print_routes(Node *n);
Route* find_route(Node *n, char *dest);

#endif