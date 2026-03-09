#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include "net.h"

int tcp_server(int port){

    int s;
    struct sockaddr_in addr;

    s=socket(AF_INET,SOCK_STREAM,0);

    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;

    bind(s,(struct sockaddr*)&addr,sizeof(addr));
    listen(s,5);

    return s;
}

int tcp_connect(char *ip,int port){

    int s;
    struct sockaddr_in addr;

    s=socket(AF_INET,SOCK_STREAM,0);

    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    inet_aton(ip,&addr.sin_addr);

    connect(s,(struct sockaddr*)&addr,sizeof(addr));

    return s;
}

int send_msg(int sock,char *msg){

    return write(sock,msg,strlen(msg));
}

int recv_msg(int sock,char *buf){

    int n=read(sock,buf,512);

    if(n>0) buf[n]=0;

    return n;
}