#ifndef NET_H
#define NET_H

int tcp_server(int port);
int tcp_connect(char *ip,int port);

int send_msg(int sock,char *msg);
int recv_msg(int sock,char *buf);
int udp_comm(char *ip, int port, char *msg, char *res);
#endif


