#ifndef MGCORE_SAFEBOOT_H
#define MGCORE_SAFEBOOT_H

#include "mgcore/types.h"

#define SAFEBOOT_ACTION_REBOOT 0
#define SAFEBOOT_ACTION_SHUTDOWN 1

void safeboot_set_action(int action);
int safeboot_get_action(void);
void safeboot_trigger(const char *reason, pid_t suspect_pid) __attribute__((noreturn));

#endif
