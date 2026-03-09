#include <stdio.h>
#include <string.h>
#include "node.h"

void node_init(Node *n, char *id, char *ip, int port) {
    strcpy(n->id, id);
    strcpy(n->ip, ip);      // Guarda o IP passado no comando de invocação [cite: 126]
    n->port = port;
    n->net = 0;             // Inicializado a 0, será atualizado no comando 'join' [cite: 135]

    n->neighbor_count = 0;
    n->route_count = 0;

    // Inicializa a distância a si próprio como 0 [cite: 56]
    add_route(n, id, id, 0); 
}
Route* find_route(Node *n, char *dest) {
    for (int i = 0; i < n->route_count; i++) {
        if (strcmp(n->routes[i].dest, dest) == 0) {
            return &n->routes[i];
        }
    }
    return NULL;
}

void add_neighbor(Node *n,char *id,char *ip,int port){

    Neighbor *nb=&n->neighbors[n->neighbor_count++];

    strcpy(nb->id,id);
    strcpy(nb->ip,ip);
    nb->port=port;
    nb->socket=-1;
}

void add_route(Node *n,char *dest,char *next,int cost){

    for(int i=0;i<n->route_count;i++){

        if(strcmp(n->routes[i].dest,dest)==0){

            if(cost<n->routes[i].cost){

                strcpy(n->routes[i].next,next);
                n->routes[i].cost=cost;
            }
            return;
        }
    }

    Route *r=&n->routes[n->route_count++];

    strcpy(r->dest,dest);
    strcpy(r->next,next);
    r->cost=cost;
}

char* get_next_hop(Node *n,char *dest){

    for(int i=0;i<n->route_count;i++){

        if(strcmp(n->routes[i].dest,dest)==0)
            return n->routes[i].next;
    }

    return NULL;
}

void print_routes(Node *n){

    printf("Routing table\n");

    for(int i=0;i<n->route_count;i++){

        printf("%s -> %s cost=%d\n",
        n->routes[i].dest,
        n->routes[i].next,
        n->routes[i].cost);
    }
}