#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include "message.h"
#include "routing.h"
#include "node.h"
#include "net.h"

int monitor = 0;

void perform_leave(Node *node, char *regIP, int regUDP) {
    if (node->net == 0) return;
    char msg[128], res[1024];
    int tid = rand() % 1000;
    sprintf(msg,"REG %03d 3 %03d %s\n", tid, node->net, node->id);
    udp_comm(regIP, regUDP, msg, res);

    for(int i=0; i<node->neighbor_count; i++){
        if(node->neighbors[i].socket > 0){
            close(node->neighbors[i].socket);
            node->neighbors[i].socket = -1;
        }
    }
    node->neighbor_count = 0;
    node->route_count = 0;
    node->net = 0;
    printf("Left network\n");
}

void send_neighbor_hello(int sock, char *id){
    char msg[64];
    sprintf(msg, "NEIGHBOR %s\n", id);
    send_msg(sock, msg);
}

void forward_chat(Node *node, char *origin, char *dest, char *msg){
    char *next = get_next_hop(node, dest);
    if(next == NULL){
        printf("No route to %s\n", dest);
        return;
    }
    for(int i=0; i<node->neighbor_count; i++){
        if(strcmp(node->neighbors[i].id, next) == 0){
            char buffer[256];
            sprintf(buffer, "CHAT %s %s %s\n", origin, dest, msg);
            send_msg(node->neighbors[i].socket, buffer);
            return;
        }
    }
}

int main(int argc, char *argv[]){
    if(argc < 3){
        printf("usage: OWR IP TCP [regIP regUDP]\n");
        return 0;
    }
    srand(time(NULL));
    char *myIP = argv[1];
    int myPort = atoi(argv[2]);
    char *regIP = (argc > 3) ? argv[3] : "193.136.138.142";
    int regUDP = (argc > 4) ? atoi(argv[4]) : 59000;

    Node node;
    node_init(&node, "99", myIP, myPort);
    node.server_socket = tcp_server(node.port);
    printf("Node running on %s:%d\n", node.ip, node.port);

    fd_set readfds;
    while(1){
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);
        FD_SET(node.server_socket, &readfds);
        int maxfd = node.server_socket;

        for(int i=0; i<node.neighbor_count; i++){
            if(node.neighbors[i].socket > 0){
                FD_SET(node.neighbors[i].socket, &readfds);
                if(node.neighbors[i].socket > maxfd) maxfd = node.neighbors[i].socket;
            }
        }

        select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if(FD_ISSET(0, &readfds)){
            char cmd[256];
            if(fgets(cmd, 256, stdin) == NULL) break;


            /* remove edge (re) id */
            else if(strncmp(cmd, "re", 2) == 0) {
                char target_id[MAX_ID];
                if(sscanf(cmd, "%*s %s", target_id) == 1) {
                    for(int i = 0; i < node.neighbor_count; i++) {
                        if(strcmp(node.neighbors[i].id, target_id) == 0 && node.neighbors[i].socket > 0) {
                            close(node.neighbors[i].socket);
                            node.neighbors[i].socket = -1;
                            // Notifica o protocolo para atualizar o custo para infinito
                            handle_edge_failure(&node, target_id); 
                            printf("Edge with %s removed.\n", target_id);
                            break;
                        }
                    }
                }
            }




            if(strncmp(cmd, "j ", 2) == 0){
                int net; char id[3];
                if(sscanf(cmd, "%*s %d %s", &net, id) == 2){
                    node.net = net; strcpy(node.id, id);
                    node.route_count = 0;
                    char msg[128], res[1024];
                    sprintf(msg, "REG %03d 0 %03d %s %s %d\n", rand()%1000, node.net, node.id, node.ip, node.port);
                    udp_comm(regIP, regUDP, msg, res);
                    printf("Joined network\n");
                    add_route(&node, node.id, node.id, 0); // Garante a rota própria
                }
            }
            else if(strncmp(cmd, "dj", 2) == 0){
                int net; char id[3];
                sscanf(cmd, "%*s %d %s", &net, id);
                node.net = net; strcpy(node.id, id);
                add_route(&node, node.id, node.id, 0);
                printf("Direct join OK\n");
            }
            else if(strncmp(cmd, "n ", 2) == 0){
                int net; sscanf(cmd, "%*s %d", &net);
                char msg[128], res[2048];
                sprintf(msg, "NODES %03d 0 %03d\n", rand()%1000, net);
                udp_comm(regIP, regUDP, msg, res);
                printf("%s\n", res);
            }
            else if(strncmp(cmd, "sg", 2) == 0){
                printf("Neighbors:\n");
                for(int i=0; i<node.neighbor_count; i++){
                    if(node.neighbors[i].socket > 0)
                        printf("%s (%s:%d)\n", node.neighbors[i].id, node.neighbors[i].ip, node.neighbors[i].port);
                }
            }
            else if(strncmp(cmd, "ae", 2) == 0){
                char id[3]; sscanf(cmd, "%*s %s", id);
                char msg[128], res[1024];
                sprintf(msg, "CONTACT %03d 0 %03d %s\n", rand()%1000, node.net, id);
                if (udp_comm(regIP, regUDP, msg, res) > 0) {
                    char type[16], tip[64], rid[MAX_ID]; int rtid, rop, rnet, tport;
                    if (sscanf(res, "%s %d %d %d %s %s %d", type, &rtid, &rop, &rnet, rid, tip, &tport) >= 7 && rop == 1) {
                        int s = tcp_connect(tip, tport);
                        if (s > 0) {
                            add_neighbor(&node, rid, tip, tport);
                            node.neighbors[node.neighbor_count - 1].socket = s;
                            send_neighbor_hello(s, node.id);
                            printf("Edge established with %s\n", rid);
                        }
                    }
                }
            } // Fecho correto do AE
            else if(strncmp(cmd, "re", 2) == 0){
                char id[3]; sscanf(cmd, "%*s %s", id);
                for(int i=0; i<node.neighbor_count; i++){
                    if(strcmp(node.neighbors[i].id, id) == 0 && node.neighbors[i].socket > 0){
                        close(node.neighbors[i].socket);
                        node.neighbors[i].socket = -1;
                        handle_edge_failure(&node, id);
                        printf("Edge removed\n");
                    }
                }
            }
            else if(strncmp(cmd, "a", 1) == 0){
                add_route(&node, node.id, node.id, 0);
                broadcast_routes(&node);
            }
            else if(strncmp(cmd, "sr", 2) == 0){
                print_routes(&node); // Corrigido de 's' para 'print_routes'
            }
            else if(strncmp(cmd, "sm", 2) == 0){
                monitor = 1; printf("Monitor ON\n");
            }
            else if(strncmp(cmd, "em", 2) == 0){
                monitor = 0; printf("Monitor OFF\n");
            }
            else if(strncmp(cmd, "m ", 2) == 0){
                char dest[3], text[128];
                if(sscanf(cmd, "%*s %s %[^\n]", dest, text) == 2)
                    forward_chat(&node, node.id, dest, text);
            }
            else if(strncmp(cmd, "l", 1) == 0) perform_leave(&node, regIP, regUDP);
            else if(strncmp(cmd, "x", 1) == 0) { perform_leave(&node, regIP, regUDP); exit(0); }
        }

        /* NEW TCP CONNECTION */
        if(FD_ISSET(node.server_socket, &readfds)){
            struct sockaddr_in cl; socklen_t len = sizeof(cl);
            int s = accept(node.server_socket, (struct sockaddr*)&cl, &len);
            if(s > 0){
                add_neighbor(&node, "unknown", inet_ntoa(cl.sin_addr), ntohs(cl.sin_port));
                node.neighbors[node.neighbor_count-1].socket = s;
                send_neighbor_hello(s, node.id);
            }
        }

        /* NEIGHBOR MESSAGES */
        for(int i=0; i<node.neighbor_count; i++){
            int s = node.neighbors[i].socket;
            if(s > 0 && FD_ISSET(s, &readfds)){
                char buf[256];
                if(recv_msg(s, buf) <= 0){
                    char id[3]; strcpy(id, node.neighbors[i].id);
                    close(s); node.neighbors[i].socket = -1;
                    handle_edge_failure(&node, id);
                } else {
                    char type[32]; sscanf(buf, "%s", type);
                    if(strcmp(type, "CHAT") == 0){
                        char o[3], d[3], txt[128];
                        sscanf(buf, "%*s %s %s %[^\n]", o, d, txt);
                        if(strcmp(d, node.id) == 0) printf("From %s: %s\n", o, txt);
                        else forward_chat(&node, o, d, txt);
                    }
                    else if(strcmp(type, "ROUTE") == 0){
                        char dest[3]; int n; sscanf(buf, "%*s %s %d", dest, &n);
                        process_route_update(&node, node.neighbors[i].id, dest, n);
                    }
                    else if(strcmp(type, "COORD") == 0){
                        char dest[3]; sscanf(buf, "%*s %s", dest);
                        process_coord_msg(&node, node.neighbors[i].id, dest);
                    }
                    else if(strcmp(type, "UNCOORD") == 0){
                        char dest[3]; sscanf(buf, "%*s %s", dest);
                        process_uncoord_msg(&node, node.neighbors[i].id, dest);
                    }
                    else if(strcmp(type, "NEIGHBOR") == 0){
                        char id[3]; sscanf(buf, "%*s %s", id);
                        strcpy(node.neighbors[i].id, id);
                    }
                }
            }
        }
    } // Fim do while
    return 0;
} // Fim do main