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


// Função auxiliar para o comando Leave (usada no 'l' e no 'x') [cite: 142, 189]
void perform_leave(Node *node, char *regIP, int regUDP) {
    if (node->net == 0) return; // Não está numa rede

    char msg[128], res[1024];
    int tid = rand() % 1000;
    // REG tid 3 net id (Solicita remoção do registo) [cite: 189]
    sprintf(msg, "REG %03d 3 %03d %s\n", tid, node->net, node->id);
    
    if (udp_comm(regIP, regUDP, msg, res) > 0) {
        printf("Server response: %s", res);
    }

    // Fecha todas as ligações TCP com vizinhos [cite: 142]
    for (int i = 0; i < node->neighbor_count; i++) {
        if (node->neighbors[i].socket > 0) {
            close(node->neighbors[i].socket);
            node->neighbors[i].socket = -1;
        }
    }
    node->neighbor_count = 0;
    node->route_count = 0;
    node->net = 0; // Reset da rede local
    printf("Left network successfully.\n");
}

// Função para identificar o nó após a criação de uma aresta [cite: 192, 196]
void send_neighbor_hello(int sock, char *my_id) {
    char hello[64];
    sprintf(hello, "NEIGHBOR %s\n", my_id);
    send_msg(sock, hello);
}

// Encaminha mensagens de chat pelo caminho mais curto [cite: 17, 204]
void forward_chat(Node *node, char *origin, char *dest, char *msg) {
    char *next = get_next_hop(node, dest);
    if (next == NULL) {
        printf("No route to %s\n", dest);
        return;
    }

    for (int i = 0; i < node->neighbor_count; i++) {
        if (strcmp(node->neighbors[i].id, next) == 0) {
            char buffer[512];
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
    char *regIP = (argc > 3) ? argv[3] : "193.136.138.142";
    int regUDP = (argc > 4) ? atoi(argv[4]) : 59000;

    Node node;
    node_init(&node, "99", myIP, myPort); 
    node.server_socket = tcp_server(node.port);
    printf("Node running on %s:%d\n", node.ip, node.port);

    fd_set readfds;

    while(1){
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // stdin
        FD_SET(node.server_socket, &readfds);

        int maxfd = node.server_socket;
        for (int i = 0; i < node.neighbor_count; i++) {
            if (node.neighbors[i].socket > maxfd) maxfd = node.neighbors[i].socket;
            if (node.neighbors[i].socket > 0) FD_SET(node.neighbors[i].socket, &readfds);
        }

        select(maxfd + 1, &readfds, NULL, NULL, NULL);


        // --- 1. COMANDOS DO UTILIZADOR (stdin) ---
        if (FD_ISSET(0, &readfds)) {
            char cmd[256];
            if (fgets(cmd, 256, stdin) == NULL) break;

            // join (j) net id [cite: 133]
            if (strncmp(cmd, "j", 1) == 0) {
                int net_val;
                char new_id[MAX_ID];
                if (sscanf(cmd, "%*s %d %s", &net_val, new_id) == 2) {
                    node.net = net_val;
                    strcpy(node.id, new_id);
                    node.route_count = 0; // Limpa o ID temporário "99"
                    add_route(&node, node.id, node.id, 0);

                    char reg_msg[128], res[1024];
                    // REG tid 0 net id IP TCP (Solicita registo) [cite: 186]
                    sprintf(reg_msg, "REG %03d 0 %03d %s %s %d\n", rand()%1000, node.net, node.id, node.ip, node.port);
                    if (udp_comm(regIP, regUDP, reg_msg, res) > 0) {
                        printf("Joined network %03d as node %s. Server: %s", node.net, node.id, res);
                    }
                }
            }
            // show nodes (n) net [cite: 137]
            else if (strncmp(cmd, "n", 1) == 0) {
                int target_net;
                if (sscanf(cmd, "%*s %d", &target_net) == 1) {
                    char msg[128], res[2048];
                    // NODES tid 0 net (Solicita lista de nós) [cite: 174]
                    sprintf(msg, "NODES %03d 0 %03d\n", rand()%1000, target_net);
                    if (udp_comm(regIP, regUDP, msg, res) > 0) {
                        printf("Nodes in network %03d:\n%s", target_net, res);
                    }
                }
            }
            // leave (l) [cite: 139]
            else if (strncmp(cmd, "l", 1) == 0) {
                perform_leave(&node, regIP, regUDP);
            }
            // exit (x) [cite: 143]
            else if (strncmp(cmd, "x", 1) == 0 || strncmp(cmd, "exit", 4) == 0) {
                perform_leave(&node, regIP, regUDP); // Leave automático antes de sair [cite: 144]
                exit(0);
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
                            int s = tcp_connect(target_ip, target_port);
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
            // announce (a) [cite: 151]
            else if (strncmp(cmd, "a", 1) == 0) {
                add_route(&node, node.id, node.id, 0); 
                broadcast_routes(&node); 
            }
            // message (m) dest message [cite: 156]
            else if (strncmp(cmd, "m", 1) == 0) {
                char dest[MAX_ID], chat_payload[128];
                if (sscanf(cmd, "%*s %s %[^\n]", dest, chat_payload) == 2) {
                    forward_chat(&node, node.id, dest, chat_payload);
                }
            }
            else if (strncmp(cmd, "routes", 6) == 0 || strncmp(cmd, "sr", 2) == 0) {
                print_routes(&node);
            }
        }
        
        // --- 2. NOVAS CONEXÕES TCP (accept) [cite: 232] ---
        if (FD_ISSET(node.server_socket, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int s = accept(node.server_socket, (struct sockaddr*)&client_addr, &addr_len);
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
                if (recv_msg(s, buf) <= 0) {
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
                            process_coord_msg(&node, node.neighbors[i].id, arg1);
                        }
                        else if (strcmp(type, "UNCOORD") == 0) {
                            process_uncoord_msg(&node, node.neighbors[i].id, arg1);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
