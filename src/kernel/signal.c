#include "mgcore/signal.h"
#include "mgcore/task.h"

void signal_init(void) {
}

int signal_send(pid_t pid, int signum) {
  (void)signum;
  return task_find(pid) ? 0 : -1;
}
