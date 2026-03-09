#ifndef MESSAGE_H
#define MESSAGE_H

void send_chat(int sock,char *dest,char *msg);
void send_route_update(int sock,char *dest,int cost);

#endif