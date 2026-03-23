#include "mgcore/safeboot.h"

#include "mgcore/console.h"
#include "mgcore/interrupt.h"
#include "mgcore/kernel.h"
#include "mgcore/task.h"

static int g_safeboot_action = SAFEBOOT_ACTION_REBOOT;

void safeboot_set_action(int action) {
  if (action == SAFEBOOT_ACTION_REBOOT || action == SAFEBOOT_ACTION_SHUTDOWN) {
    g_safeboot_action = action;
  }
}

int safeboot_get_action(void) {
  return g_safeboot_action;
}

void safeboot_trigger(const char *reason, pid_t suspect_pid) {
  interrupt_disable();
  console_printf("safeboot: reason=%s\n", reason ? reason : "unknown");

  if (suspect_pid > 1) {
    if (task_kill(suspect_pid, 137)) {
      console_printf("safeboot: killed suspect pid=%u\n", (unsigned)suspect_pid);
    } else {
      console_printf("safeboot: suspect pid=%u not killable\n", (unsigned)suspect_pid);
    }
  }

  if (g_safeboot_action == SAFEBOOT_ACTION_SHUTDOWN) {
    console_writeln("safeboot: policy=shutdown");
    kernel_halt();
  }

  console_writeln("safeboot: policy=reboot");
  kernel_reboot();
}
