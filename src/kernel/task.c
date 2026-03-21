#include "mgcore/console.h"
#include "mgcore/kstring.h"
#include "mgcore/task.h"

static task g_tasks[MGCORE_MAX_TASKS];
static pid_t g_next_pid = 1;
static int g_current_index;
static int g_task_count;

static task *task_alloc(const char *name, task_state state) {
  size_t i;
  for (i = 0; i < MGCORE_ARRAY_SIZE(g_tasks); ++i) {
    task *entry = &g_tasks[i];
    if (entry->state == TASK_UNUSED) {
      entry->pid = g_next_pid++;
      entry->ppid = 0;
      entry->state = state;
      entry->runtime_ticks = 0;
      entry->wakeup_tick = 0;
      entry->exit_code = 0;
      kstrncpy(entry->name, name, sizeof(entry->name) - 1);
      entry->name[sizeof(entry->name) - 1] = '\0';
      kstrcpy(entry->cwd, "/");
      vmm_address_space_init(&entry->mm);
      ++g_task_count;
      return entry;
    }
  }
  return NULL;
}

void task_init(void) {
  kmemset(g_tasks, 0, sizeof(g_tasks));
  g_current_index = 0;
  g_task_count = 0;
  task_alloc("bootstrap", TASK_RUNNING);
}

task *task_current(void) {
  return &g_tasks[g_current_index];
}

task *task_create_kernel(const char *name) {
  return task_alloc(name, TASK_RUNNABLE);
}

task *task_spawn_user(const char *name) {
  task *child = task_alloc(name, TASK_RUNNABLE);
  if (child) {
    child->ppid = task_current()->pid;
  }
  return child;
}

task *task_find(pid_t pid) {
  size_t i;
  for (i = 0; i < MGCORE_ARRAY_SIZE(g_tasks); ++i) {
    if (g_tasks[i].state != TASK_UNUSED && g_tasks[i].pid == pid) {
      return &g_tasks[i];
    }
  }
  return NULL;
}

pid_t task_current_pid(void) {
  return task_current()->pid;
}

void task_exit(int status) {
  task *current = task_current();
  current->state = TASK_ZOMBIE;
  current->exit_code = status;
}

void task_sleep_until(uint64_t tick) {
  task *current = task_current();
  current->state = TASK_SLEEPING;
  current->wakeup_tick = tick;
}

void task_scheduler_tick(uint64_t now) {
  size_t i;
  int next = g_current_index;
  task *current = task_current();

  if (current->state == TASK_RUNNING) {
    current->runtime_ticks++;
    current->state = TASK_RUNNABLE;
  }

  for (i = 0; i < MGCORE_ARRAY_SIZE(g_tasks); ++i) {
    if (g_tasks[i].state == TASK_SLEEPING && g_tasks[i].wakeup_tick <= now) {
      g_tasks[i].state = TASK_RUNNABLE;
    }
  }

  for (i = 0; i < MGCORE_ARRAY_SIZE(g_tasks); ++i) {
    int candidate = (g_current_index + 1 + (int)i) % (int)MGCORE_ARRAY_SIZE(g_tasks);
    if (g_tasks[candidate].state == TASK_RUNNABLE) {
      next = candidate;
      break;
    }
  }

  g_current_index = next;
  if (g_tasks[g_current_index].state == TASK_RUNNABLE) {
    g_tasks[g_current_index].state = TASK_RUNNING;
  }
}

void task_dump(void) {
  size_t i;
  console_writeln("pid  ppid state     ticks name");
  for (i = 0; i < MGCORE_ARRAY_SIZE(g_tasks); ++i) {
    const task *entry = &g_tasks[i];
    const char *state = "unused";
    if (entry->state == TASK_UNUSED) {
      continue;
    }
    if (entry->state == TASK_RUNNABLE) state = "ready";
    else if (entry->state == TASK_RUNNING) state = "run";
    else if (entry->state == TASK_SLEEPING) state = "sleep";
    else if (entry->state == TASK_ZOMBIE) state = "zombie";
    console_printf("%u    %u    %s    %u   %s\n",
                   (unsigned)entry->pid,
                   (unsigned)entry->ppid,
                   state,
                   (unsigned)entry->runtime_ticks,
                   entry->name);
  }
}
