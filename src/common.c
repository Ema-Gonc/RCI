#include "../include/common.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern AppConfig config;

int send_msg(int fd, const char *msg) {
  size_t len = strlen(msg);
  const char *ptr = msg;

  while (len > 0) {
    ssize_t written = write(fd, ptr, len);
    if (written <= 0) {
      return -1;
    }
    ptr += written;
    len -= (size_t)written;
  }

  return 0;
}

int format_id(char out[3], int id) {
  if (id < 0 || id > 99) {
    return -1;
  }

  out[0] = (char)('0' + (id / 10));
  out[1] = (char)('0' + (id % 10));
  out[2] = '\0';
  return 0;
}

void monitor_log(const char *format, ...) {
  if (!config.monitor)
    return;

  va_list console_args;
  va_start(console_args, format);

  va_list file_args;
  va_copy(file_args, console_args);

  vprintf(format, console_args);
  fflush(stdout);
  va_end(console_args);

  FILE *f = fopen("/tmp/owr_monitor.log", "a");
  if (f) {
    vfprintf(f, format, file_args);
    fclose(f);
  }

  va_end(file_args);
}