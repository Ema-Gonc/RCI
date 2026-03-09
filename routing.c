#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "node.h"
#include "net.h"

void process_route(Node *node,char *neighbor,char *dest,int cost){

    int new_cost=cost+1;

    add_route(node,dest,neighbor,new_cost);
}

void broadcast_routes(Node *node){

    char msg[256];

    for(int r=0;r<node->route_count;r++){

        sprintf(msg,"ROUTE %s %d\n",
        node->routes[r].dest,
        node->routes[r].cost);

        for(int n=0;n<node->neighbor_count;n++){

            if(node->neighbors[n].socket>0)
                write(node->neighbors[n].socket,msg,strlen(msg));
        }
    }
}


void send_routes(Node *node){

    char buffer[256];

    for(int r=0;r<node->route_count;r++){

        sprintf(buffer,"ROUTE %s %d\n",
        node->routes[r].dest,
        node->routes[r].cost);

        for(int n=0;n<node->neighbor_count;n++){

            if(node->neighbors[n].socket>0)
                send_msg(node->neighbors[n].socket,buffer);
        }
    }
}


void process_route_update(Node *node,char *neighbor,char *dest,int cost){

    int new_cost = cost + 1;

    add_route(node,dest,neighbor,new_cost);
}


