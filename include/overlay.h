#ifndef OVERLAY_H
#define OVERLAY_H

#include "common.h"
#include <sys/types.h>
#include <sys/select.h>

typedef struct
{
    int id;
    int fd;
    char ip[64];
    char tcp[32];
} Neighbor;

extern Neighbor neighbors[MAX_NODES];

int o_tcp_listener_init(const char *ip, const char *port);

int o_connect_out(const char *target_ip, const char *target_port, int target_id, int my_id);
void o_accept_in(int listen_fd);

void o_init_nb(void);
void o_add_nb(int id, int fd, const char *ip, const char *tcp);
void o_read_nb(fd_set *read_fds);
void o_rm_nb(int id);
void o_rm_all_nb(void);

int check_nb_max_fd(fd_set *read_fds, int max_fd);

#endif
