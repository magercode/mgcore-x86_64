#ifndef MGCORE_COMMANDS_H
#define MGCORE_COMMANDS_H

typedef int (*command_fn)(int argc, char **argv);

typedef struct command_def {
  const char *name;
  const char *help;
  command_fn fn;
} command_def;

const command_def *commands_get_all(int *count_out);

#endif
