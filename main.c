#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "message.h"
#include "routing.h"
#include "node.h"
#include "net.h"

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

void show_neighbors(Node *node){

    printf("Neighbors:\n");

    for(int i=0;i<node->neighbor_count;i++){

        printf("%s %s %d\n",
        node->neighbors[i].id,
        node->neighbors[i].ip,
        node->neighbors[i].port);
    }
}

void show_nodes(Node *node){

    printf("Known nodes:\n");

    for(int i=0;i<node->route_count;i++){

        printf("%s\n",node->routes[i].dest);
    }
}

void remove_edge(Node *node,char *id){

    for(int i=0;i<node->neighbor_count;i++){

        if(strcmp(node->neighbors[i].id,id)==0){

            close(node->neighbors[i].socket);

            for(int j=i;j<node->neighbor_count-1;j++)
                node->neighbors[j]=node->neighbors[j+1];

            node->neighbor_count--;

            printf("Edge removed: %s\n",id);

            return;
        }
    }

    printf("Neighbor not found\n");
}

int main(int argc,char *argv[]){

    if(argc<3){

        printf("usage: node id port\n");
        return 0;
    }

    Node node;

    node_init(&node,argv[1],atoi(argv[2]));

    node.server_socket=tcp_server(node.port);

    printf("Node %s running\n",node.id);

    fd_set readfds;

    while(1){

        FD_ZERO(&readfds);

        FD_SET(0,&readfds);
        FD_SET(node.server_socket,&readfds);

        int maxfd=node.server_socket;

        for(int i=0;i<node.neighbor_count;i++){

            int s=node.neighbors[i].socket;

            if(s>0){

                FD_SET(s,&readfds);

                if(s>maxfd) maxfd=s;
            }
        }

        select(maxfd+1,&readfds,NULL,NULL,NULL);

        if(FD_ISSET(0,&readfds)){

            char cmd[256];

            fgets(cmd,256,stdin);

            if(strncmp(cmd,"join",4)==0){

                char id[32],ip[64];
                int port;

                sscanf(cmd,"join %s %s %d",id,ip,&port);

                int s=tcp_connect(ip,port);

                add_neighbor(&node,id,ip,port);

                node.neighbors[node.neighbor_count-1].socket=s;
                add_route(&node,id,id,1);
                printf("Connected to %s\n",id);
            }

            else if(strncmp(cmd,"add edge",8)==0){

                char id[32],ip[64];
                int port;

                sscanf(cmd,"add edge %s %s %d",id,ip,&port);

                int s=tcp_connect(ip,port);

                add_neighbor(&node,id,ip,port);

                node.neighbors[node.neighbor_count-1].socket=s;
                add_route(&node,id,id,1);
                printf("Edge created with %s\n",id);
            }

            else if(strncmp(cmd,"remove edge",11)==0){

                char id[32];

                sscanf(cmd,"remove edge %s",id);

                remove_edge(&node,id);
            }

            else if(strncmp(cmd,"show neighbors",14)==0){

                show_neighbors(&node);
            }

            else if(strncmp(cmd,"show nodes",10)==0){

                show_nodes(&node);
            }

            else if(strncmp(cmd,"chat",4)==0){

                char dest[32];
                char msg[256];

                sscanf(cmd,"chat %s %[^\n]",dest,msg);

                forward_chat(&node,dest,msg);
            }

            else if(strncmp(cmd,"leave",5)==0){

                for(int i=0;i<node.neighbor_count;i++){

                    close(node.neighbors[i].socket);
                }

                node.neighbor_count=0;

                printf("Left overlay network\n");
            }

            else if(strncmp(cmd,"exit",4)==0){

                printf("Exiting...\n");

                exit(0);
            }

            else if(strncmp(cmd,"routes",6)==0){

                print_routes(&node);
            }
        }

        if(FD_ISSET(node.server_socket,&readfds)){

            struct sockaddr_in client_addr;

            socklen_t addr_len=sizeof(client_addr);

            int s=accept(node.server_socket,(struct sockaddr*)&client_addr,&addr_len);

            printf("New node connected (socket %d)\n",s);
        }

        for(int i=0;i<node.neighbor_count;i++){

            int s=node.neighbors[i].socket;

            if(s>0 && FD_ISSET(s,&readfds)){

                char buf[512];

                int n=recv_msg(s,buf);

                if(n<=0){

                    close(s);

                    node.neighbors[i].socket=-1;
                }

                else{

                    char type[32],dest[32],msg[256];

                    if(sscanf(buf,"%s %s",type,dest)>=2){

                        if(strcmp(type,"CHAT")==0){

                            sscanf(buf,"%*s %*s %[^\n]",msg);

                            if(strcmp(dest,node.id)==0){

                                printf("Message received: %s\n",msg);
                            }

                            else{

                                forward_chat(&node,dest,msg);
                            }
                        }

                        else if(strcmp(type,"ROUTE")==0){

                            int cost;

                            sscanf(buf,"%*s %s %d",dest,&cost);

                            process_route_update(&node,node.neighbors[i].id,dest,cost);
                        }
                    }
                }
            }
        }
    }

    return 0;
}