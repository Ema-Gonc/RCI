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

// Função para identificar o nó após a criação de uma aresta
void send_neighbor_hello(int sock, char *my_id) {
    char hello[64];
    sprintf(hello, "NEIGHBOR %s\n", my_id); // Formato exigido [cite: 195]
    send_msg(sock, hello);
}

// Encaminha mensagens de chat pelo caminho mais curto [cite: 17, 207]
void forward_chat(Node *node, char *origin, char *dest, char *msg) {
    char *next = get_next_hop(node, dest);
    if (next == NULL) {
        printf("No route to %s\n", dest);
        return;
    }

    for (int i = 0; i < node->neighbor_count; i++) {
        if (strcmp(node->neighbors[i].id, next) == 0) {
            char buffer[512];
            // Formato: CHAT origin dest chat<LF> [cite: 206]
            sprintf(buffer, "CHAT %s %s %s\n", origin, dest, msg);
            send_msg(node->neighbors[i].socket, buffer);
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    // Invocação: OWR IP TCP [regIP regUDP] [cite: 125]
    if (argc < 3) {
        printf("usage: %s IP TCP [regIP regUDP]\n", argv[0]);
        return 0;
    }

    srand(time(NULL));
    char *myIP = argv[1];
    int myPort = atoi(argv[2]);
    char *regIP = (argc > 3) ? argv[3] : "193.136.138.142"; // [cite: 128]
    int regUDP = (argc > 4) ? atoi(argv[4]) : 59000;

    Node node;
    node_init(&node, "99", myIP, myPort); 
    node.server_socket = tcp_server(node.port); // [cite: 232]
    printf("Node running on %s:%d\n", node.ip, node.port);

    fd_set readfds;

    while(1){
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // stdin [cite: 227]
        FD_SET(node.server_socket, &readfds);

        int maxfd = node.server_socket;
        for (int i = 0; i < node.neighbor_count; i++) {
            int s = node.neighbors[i].socket;
            if (s > 0) {
                FD_SET(s, &readfds);
                if (s > maxfd) maxfd = s;
            }
        }

        // Multiplexagem de entradas síncronas 
        select(maxfd + 1, &readfds, NULL, NULL, NULL);

        // --- 1. COMANDOS DO UTILIZADOR ---
        if (FD_ISSET(0, &readfds)) {
            char cmd[256];
            if (fgets(cmd, 256, stdin) == NULL) break;

            // join (j) net id [cite: 132]
            if (strncmp(cmd, "j", 1) == 0) {
                int net_val;
                char new_id[MAX_ID];
                if (sscanf(cmd, "%*s %d %s", &net_val, new_id) == 2) {
                    node.net = net_val;
                    strcpy(node.id, new_id);
                    // Registo UDP no servidor [cite: 182]
                    char reg_msg[128], res[1024];
                    sprintf(reg_msg, "REG %03d 0 %03d %s %s %d\n", rand()%1000, node.net, node.id, node.ip, node.port);
                    udp_comm(regIP, regUDP, reg_msg, res); 
                }
            }
            // add edge (ae) id [cite: 144]
            else if (strncmp(cmd, "ae", 2) == 0) {
                char target_id[MAX_ID];
                sscanf(cmd, "%*s %s", target_id);
                
                char contact_msg[128], res[1024];
                sprintf(contact_msg, "CONTACT %03d 0 %03d %s\n", rand()%1000, node.net, target_id); 
                
                if (udp_comm(regIP, regUDP, contact_msg, res) > 0) {
                    char resp_type[16], target_ip[64], r_id[MAX_ID];
                    int r_tid, r_op, r_net, target_port;
                    if (sscanf(res, "%s %d %d %d %s %s %d", resp_type, &r_tid, &r_op, &r_net, r_id, target_ip, &target_port) >= 7) {
                        if (r_op == 1) {
                            int s = tcp_connect(target_ip, target_port); // 
                            if (s > 0) {
                                add_neighbor(&node, target_id, target_ip, target_port);
                                node.neighbors[node.neighbor_count - 1].socket = s;
                                send_neighbor_hello(s, node.id); 
                                printf("Edge established with %s\n", target_id);
                            }
                        }
                    }
                }
            }
            // announce (a) [cite: 150, 151]
            else if (strncmp(cmd, "a", 1) == 0) {
                add_route(&node, node.id, node.id, 0); 
                broadcast_routes(&node); // Envia ROUTE [cite: 199]
            }
            // message (m) dest message [cite: 156]
            else if (strncmp(cmd, "m", 1) == 0) {
                char dest[MAX_ID], chat_payload[128];
                if (sscanf(cmd, "%*s %s %[^\n]", dest, chat_payload) == 2) {
                    forward_chat(&node, node.id, dest, chat_payload);
                }
            }
            else if (strncmp(cmd, "exit", 4) == 0 || strncmp(cmd, "x", 1) == 0) {
                exit(0); // [cite: 142]
            }
        }

        // --- 2. NOVAS CONEXÕES TCP ---
        if (FD_ISSET(node.server_socket, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int s = accept(node.server_socket, (struct sockaddr*)&client_addr, &addr_len); // [cite: 232]
            if (s > 0) {
                add_neighbor(&node, "unknown", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                node.neighbors[node.neighbor_count - 1].socket = s;
                send_neighbor_hello(s, node.id); 
                printf("Accepted connection (socket %d)\n", s);
            }
        }

        // --- 3. MENSAGENS DOS VIZINHOS ---
        for (int i = 0; i < node.neighbor_count; i++) {
            int s = node.neighbors[i].socket;
            if (s > 0 && FD_ISSET(s, &readfds)) {
                char buf[BUFFER_SIZE];
                if (recv_msg(s, buf) <= 0) { // Falha/Remoção de aresta [cite: 76, 99]
                    char failed_id[MAX_ID];
                    strcpy(failed_id, node.neighbors[i].id);
                    close(s);
                    node.neighbors[i].socket = -1;
                    handle_edge_failure(&node, failed_id); 
                } else {
                    char type[32], arg1[MAX_ID], arg2[MAX_ID];
                    if (sscanf(buf, "%s %s", type, arg1) >= 2) {
                        if (strcmp(type, "NEIGHBOR") == 0) {
                            strcpy(node.neighbors[i].id, arg1);
                        }
                        else if (strcmp(type, "CHAT") == 0) {
                            char origin[MAX_ID], dest[MAX_ID], msg_body[128];
                            sscanf(buf, "%*s %s %s %[^\n]", origin, dest, msg_body);
                            if (strcmp(dest, node.id) == 0) printf("From %s: %s\n", origin, msg_body);
                            else forward_chat(&node, origin, dest, msg_body);
                        }
                        else if (strcmp(type, "ROUTE") == 0) {
                            sscanf(buf, "%*s %s %s", arg1, arg2);
                            process_route_update(&node, node.neighbors[i].id, arg1, atoi(arg2));
                        }
                        else if (strcmp(type, "COORD") == 0) {
                            process_coord_msg(&node, node.neighbors[i].id, arg1); // [cite: 68, 201]
                        }
                    }
                }
            }
        }
    }
    return 0;
}
