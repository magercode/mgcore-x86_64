#include "mgcore/commands.h"
#include "mgcore/console.h"
#include "mgcore/fs.h"
#include "mgcore/sched.h"
#include "mgcore/task.h"
#include "mgcore/types.h"

static int cmd_help(int argc, char **argv) {
  int count;
  int i;
  const command_def *commands;
  (void)argc;
  (void)argv;
  commands = commands_get_all(&count);
  for (i = 0; i < count; ++i) {
    console_printf("%s - %s\n", commands[i].name, commands[i].help);
  }
  return 0;
}

static int cmd_clear(int argc, char **argv) {
  (void)argc;
  (void)argv;
  console_clear();
  return 0;
}

static int cmd_echo(int argc, char **argv) {
  int i;
  for (i = 1; i < argc; ++i) {
    console_write(argv[i]);
    if (i + 1 < argc) {
      console_putc(' ');
    }
  }
  console_putc('\n');
  return 0;
}

static int cmd_sysinfo(int argc, char **argv) {
  (void)argc;
  (void)argv;
  scheduler_dump();
  return 0;
}

static int cmd_tasks(int argc, char **argv) {
  (void)argc;
  (void)argv;
  task_dump();
  return 0;
}

static int cmd_fsls(int argc, char **argv) {
  (void)argc;
  (void)argv;
  fs_dump_root();
  return 0;
}

static const command_def k_commands[] = {
  {"help", "list built-in monitor commands", cmd_help},
  {"clear", "clear VGA/serial console", cmd_clear},
  {"echo", "echo arguments", cmd_echo},
  {"sysinfo", "show scheduler uptime", cmd_sysinfo},
  {"tasks", "show current task table", cmd_tasks},
  {"fsls", "list root tmpfs entries", cmd_fsls},
};

const command_def *commands_get_all(int *count_out) {
  if (count_out) {
    *count_out = (int)MGCORE_ARRAY_SIZE(k_commands);
  }
  return k_commands;
}
