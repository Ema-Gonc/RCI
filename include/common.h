#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAX_NODES 100

typedef struct
{
    char IP[64];
    char TCP[32];
    char regIP[64];
    char regUDP[32];
    int net; // 000 to 999
    int id;  // 00 to 99
} AppConfig;

#endif