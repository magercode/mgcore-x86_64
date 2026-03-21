#ifndef MGCORE_TASK_H
#define MGCORE_TASK_H

#include "mgcore/types.h"
#include "mgcore/vmm.h"

#define MGCORE_MAX_TASKS 32
#define MGCORE_TASK_NAME_MAX 32
#define MGCORE_MAX_FDS 32

struct vfs_file;

typedef enum task_state {
  TASK_UNUSED = 0,
  TASK_RUNNABLE,
  TASK_RUNNING,
  TASK_SLEEPING,
  TASK_ZOMBIE,
} task_state;

typedef struct task {
  pid_t pid;
  pid_t ppid;
  task_state state;
  char name[MGCORE_TASK_NAME_MAX];
  uint64_t runtime_ticks;
  uint64_t wakeup_tick;
  int exit_code;
  address_space mm;
  struct vfs_file *fds[MGCORE_MAX_FDS];
  char cwd[128];
} task;

void task_init(void);
task *task_current(void);
task *task_create_kernel(const char *name);
task *task_spawn_user(const char *name);
task *task_find(pid_t pid);
pid_t task_current_pid(void);
void task_exit(int status);
void task_sleep_until(uint64_t tick);
void task_dump(void);

#endif
