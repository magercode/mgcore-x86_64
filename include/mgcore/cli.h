#ifndef MGCORE_CLI_H
#define MGCORE_CLI_H

#include <stddef.h>

#define CLI_MAX_LINE 128
#define CLI_MAX_ARGS 16

void cli_init(void);
void cli_run(void);

#endif
