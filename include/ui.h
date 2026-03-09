#ifndef UI_H
#define UI_H

#include "common.h"

// Parse and dispatch one CLI command line.
void ui_process_command(char *input, AppConfig *config, int udp_fd);

#endif