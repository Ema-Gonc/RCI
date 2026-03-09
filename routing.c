#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "node.h"
#include "net.h"
#include "routing.h"





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

void process_coord(Node *node, char *j, char *t) {
    Route *r = find_route(node, t);
    if (!r) return;

    if (r->state == 1) {
        send_uncoord(node, j, t); // Se já em coordenação, envia fim (exped/uncoord) 
    } else if (r->state == 0 && strcmp(j, r->next) != 0) {
        send_route_to_id(node, j, t, r->cost); // Responde com distância 
        send_uncoord(node, j, t);
    } else if (r->state == 0 && strcmp(j, r->next) == 0) {
        // Entra em estado de coordenação 
        r->state = 1;
        strcpy(r->succ_coord, r->next);
        r->cost = 999; // Representa infinito 
        strcpy(r->next, "-1");

        for (int i = 0; i < node->neighbor_count; i++) {
            send_coord(node, node->neighbors[i].id, t); // Avisa todos os vizinhos 
            r->coord_pending[i] = 1;
        }
    }
}

void handle_edge_failure(Node *node, char *neighbor_id) {
    for (int i = 0; i < node->route_count; i++) {
        Route *r = &node->routes[i];

        // Se o vizinho que falhou era o sucessor para este destino 
        if (strcmp(r->next, neighbor_id) == 0) {
            if (r->state == STATE_EXPEDITION) {
                r->state = STATE_COORDINATION; // Transição para coordenação 
                strcpy(r->succ_coord, "-1");   // Causa: falha de ligação 
                r->cost = INF;                 // Distância passa a infinita 
                strcpy(r->next, "-1");         // Remove o sucessor 

                // Envia mensagens de coordenação a todos os vizinhos 
                for (int j = 0; j < node->neighbor_count; j++) {
                    if (node->neighbors[j].socket > 0 && strcmp(node->neighbors[j].id, neighbor_id) != 0) {
                        send_coord(node, node->neighbors[j].id, r->dest); // Envia (coord, t) 
                        r->coord_pending[j] = 1; // Regista coordenação em curso 
                    }
                }
                check_coordination_end(node, r);
            }
        }
    }
}

void process_coord_msg(Node *node, char *j, char *t) {
    Route *r = find_route(node, t);
    if (!r) return;

    // Regra 1: Se já está em coordenação, responde com expedição 
    if (r->state == STATE_COORDINATION) {
        send_uncoord(node, j, t);
    } 
    // Regra 2: Se não é o sucessor, envia rota e fim de coordenação 
    else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) != 0) {
        char msg[128];
        sprintf(msg, "ROUTE %s %d\n", r->dest, r->cost);
        send_route_to_id(node, j, r->dest, r->cost); // Envia (route, t, dist) 
        send_uncoord(node, j, t);     // Envia (exped, t) 
    } 
    // Regra 3: Se a mensagem veio do sucessor, entra em coordenação 
    else if (r->state == STATE_EXPEDITION && strcmp(j, r->next) == 0) {
        r->state = STATE_COORDINATION;
        strcpy(r->succ_coord, j); // Guarda quem causou a coordenação 
        r->cost = INF;
        strcpy(r->next, "-1");

        for (int k = 0; k < node->neighbor_count; k++) {
            if (node->neighbors[k].socket > 0) {
                send_coord(node, node->neighbors[k].id, t); 
                r->coord_pending[k] = 1;
            }
        }
    }
}

void send_coord(Node *node, char *target_id, char *dest) {
    char msg[128];
    sprintf(msg, "COORD %s\n", dest); 
    for(int i=0; i<node->neighbor_count; i++) {
        if(strcmp(node->neighbors[i].id, target_id) == 0 && node->neighbors[i].socket > 0) {
            send_msg(node->neighbors[i].socket, msg);
        }
    }
}

void send_uncoord(Node *node, char *target_id, char *dest) {
    char msg[128];
    sprintf(msg, "UNCOORD %s\n", dest); 
    for(int i=0; i<node->neighbor_count; i++) {
        if(strcmp(node->neighbors[i].id, target_id) == 0 && node->neighbors[i].socket > 0) {
            send_msg(node->neighbors[i].socket, msg);
        }
    }
}

void send_route_to_id(Node *node, char *target_id, char *dest, int cost) {
    char msg[128];
    sprintf(msg, "ROUTE %s %d\n", dest, cost);
    
    for(int i = 0; i < node->neighbor_count; i++) {
        if(strcmp(node->neighbors[i].id, target_id) == 0 && node->neighbors[i].socket > 0) {
            send_msg(node->neighbors[i].socket, msg);
            return;
        }
    }
}

void check_coordination_end(Node *node, Route *r) {
    for (int k = 0; k < node->neighbor_count; k++) {
        if (r->coord_pending[k] == 1) return; 
    }
    r->state = STATE_EXPEDITION; 
    if (r->cost != INF) broadcast_routes(node);
    if (strcmp(r->succ_coord, "-1") != 0) send_uncoord(node, r->succ_coord, r->dest); 
}


// routing.c

void process_uncoord_msg(Node *node, char *j, char *t) {
    Route *r = find_route(node, t);
    if (!r) return;

    // 1. Se o nó está em estado de coordenação, marca que este vizinho terminou 
        // Localiza o vizinho j na lista para limpar o flag de coordenação pendente
        for (int i = 0; i < node->neighbor_count; i++) {
            if (strcmp(node->neighbors[i].id, j) == 0) {
                r->coord_pending[i] = 0; // coord[t, j] := 0 
                break;
            }
        }

        // 2. Verifica se a coordenação terminou para TODOS os vizinhos 
        int all_done = 1;
        for (int i = 0; i < node->neighbor_count; i++) {
            if (node->neighbors[i].socket > 0 && r->coord_pending[i] == 1) {
                all_done = 0;
                break;
            }
        }

        // 3. Se todos terminaram, regressa ao estado de expedição 
        if (all_done) {
            r->state = STATE_EXPEDITION;
            printf("Node returned to expedition state for destination %s\n", t);
            if (r->cost < INF) {
                broadcast_routes(node);
            }
            if (strcmp(r->succ_coord, "-1") != 0) {
                send_uncoord(node, r->succ_coord, t); 
            }
        }
    }
