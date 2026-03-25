#include "../include/net.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

int tcp_server(int port) {
    int s;
    struct sockaddr_in addr;

    s = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 5);

    return s;
}

int tcp_connect(char *ip, int port) {
    int s;
    struct sockaddr_in addr;

    s = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ip, &addr.sin_addr);

    connect(s, (struct sockaddr*)&addr, sizeof(addr));

    return s;
}

int send_msg(int sock, char *msg) {
    return write(sock, msg, strlen(msg));
}

int recv_msg(int sock, char *buf) {
    int n = read(sock, buf, 512);
    if (n > 0) buf[n] = 0;
    return n;
}

int udp_comm(char *ip, int port, char *msg, char *res) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ip, &addr.sin_addr);

    sendto(s, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr));

    struct timeval tv;
    tv.tv_sec = 2; tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recvfrom(s, res, 1024, 0, NULL, NULL);
    if (n > 0) res[n] = 0;
    close(s);
    return n;
}
