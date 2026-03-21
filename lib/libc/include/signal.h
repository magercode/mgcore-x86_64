#ifndef MGCORE_LIBC_SIGNAL_H
#define MGCORE_LIBC_SIGNAL_H

#include "sys/types.h"

typedef void (*sighandler_t)(int);

int kill(pid_t pid, int signum);
sighandler_t signal(int signum, sighandler_t handler);

#endif
