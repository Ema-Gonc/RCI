#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "message.h"
#include "routing.h"
#include "node.h"
#include "net.h"
#include <arpa/inet.h>

void forward_chat(Node *node, char *dest, char *msg) {
    char *next = get_next_hop(node, dest); 

    if (next == NULL) {
        printf("No route to %s\n", dest);
        return;
    }

    for (int i = 0; i < node->neighbor_count; i++) {
        if (strcmp(node->neighbors[i].id, next) == 0) {
            char buffer[512];
            sprintf(buffer, "CHAT %s %s\n", dest, msg);
            send_msg(node->neighbors[i].socket, buffer); 
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: node id port\n");
        return 0;
    }

    Node node;
    node_init(&node, argv[1], atoi(argv[2])); 

    node.server_socket = tcp_server(node.port); 

    printf("Node %s running\n", node.id);

    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); 
        FD_SET(node.server_socket, &readfds); 

        int maxfd = node.server_socket;

        // FIXED: Changed node-> to node.
        for (int i = 0; i < node.neighbor_count; i++) {
            int s = node.neighbors[i].socket;
            if (s > 0) {
                FD_SET(s, &readfds);
                if (s > maxfd) maxfd = s;
            }
        }

        select(maxfd + 1, &readfds, NULL, NULL, NULL); 

        if (FD_ISSET(0, &readfds)) {
            char cmd[256];
            if (fgets(cmd, 256, stdin)) {
                if (strncmp(cmd, "join", 4) == 0) {
                    char id[32], ip[64];
                    int port;
                    sscanf(cmd, "join %s %s %d", id, ip, &port);
                    int s = tcp_connect(ip, port); 
                    add_neighbor(&node, id, ip, port); 
                    node.neighbors[node.neighbor_count - 1].socket = s;
                    printf("Connected to %s\n", id);
                } else if (strncmp(cmd, "direct", 6) == 0) {
                    char id[32], ip[64];
                    int port;
                    sscanf(cmd, "direct %s %s %d", id, ip, &port);
                    int s = tcp_connect(ip, port); 
                    add_neighbor(&node, id, ip, port); 
                    node.neighbors[node.neighbor_count - 1].socket = s;
                    printf("Direct link created with %s\n", id);
                } else if (strncmp(cmd, "chat", 4) == 0) {
                    char dest[32], msg[256];
                    sscanf(cmd, "chat %s %[^\n]", dest, msg);
                    forward_chat(&node, dest, msg); 
                } else if (strncmp(cmd, "leave", 5) == 0) {
                    // FIXED: Changed node-> to node.
                    for (int i = 0; i < node.neighbor_count; i++) {
                        if (node.neighbors[i].socket > 0) close(node.neighbors[i].socket);
                    }
                    node.neighbor_count = 0;
                    printf("Left overlay network\n");
                } else if (strncmp(cmd, "routes", 6) == 0) {
                    print_routes(&node); 
                }
            }
        }

        if (FD_ISSET(node.server_socket, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int s = accept(node.server_socket, (struct sockaddr *)&client_addr, &addr_len);
            printf("New node connected (socket %d)\n", s);
            // In a full implementation, you'd handle adding this new connection to neighbors
        }

        // FIXED: Changed node-> to node.
        for (int i = 0; i < node.neighbor_count; i++) {
            int s = node.neighbors[i].socket;
            if (s > 0 && FD_ISSET(s, &readfds)) {
                char buf[512];
                int n = recv_msg(s, buf); 

                if (n <= 0) {
                    close(s);
                    node.neighbors[i].socket = -1;
                } else {
                    char type[32], dest[32], msg[256];
                    if (sscanf(buf, "%s %s", type, dest) >= 2) {
                        if (strcmp(type, "CHAT") == 0) {
                            sscanf(buf, "%*s %*s %[^\n]", msg);
                            if (strcmp(dest, node.id) == 0) {
                                printf("Message received: %s\n", msg);
                            } else {
                                forward_chat(&node, dest, msg);
                            }
                        } else if (strcmp(type, "ROUTE") == 0) {
                            int cost;
                            sscanf(buf, "%*s %s %d", dest, &cost);
                            process_route_update(&node, node.neighbors[i].id, dest, cost); 
                        }
                    }
                }
            }
        }
    }

    return 0;
}