#ifndef MGCORE_SIGNAL_H
#define MGCORE_SIGNAL_H

#include "mgcore/types.h"

#define MGCORE_SIG_DFL ((void (*)(int))0)
#define MGCORE_SIG_IGN ((void (*)(int))1)

typedef struct mgcore_sigaction {
  void (*sa_handler)(int);
  uint64_t sa_flags;
  uint64_t sa_mask;
} mgcore_sigaction;

void signal_init(void);
int signal_send(pid_t pid, int signum);

#endif
