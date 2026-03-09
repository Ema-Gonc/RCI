#include <stdio.h>
#include <string.h>
#include <unistd.h>

void send_chat(int sock,char *dest,char *msg){

    char buffer[512];

    sprintf(buffer,"CHAT %s %s\n",dest,msg);

    write(sock,buffer,strlen(buffer));
}

void send_route_update(int sock,char *dest,int cost){

    char buffer[256];

    sprintf(buffer,"ROUTE %s %d\n",dest,cost);

    write(sock,buffer,strlen(buffer));
}