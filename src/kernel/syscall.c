#include "mgcore/console.h"
#include "mgcore/errno.h"
#include "mgcore/syscall.h"

#include <stddef.h>

extern const syscall_handler_t g_syscall_table[];
extern const size_t g_syscall_table_count;

void syscall_init(void) {
  syscall_arch_init();
  console_printf("syscall: table ready (%u entries)\n", (unsigned)g_syscall_table_count);
}

long syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  if (nr >= g_syscall_table_count || !g_syscall_table[nr]) {
    return -MGCORE_ENOSYS;
  }
  return g_syscall_table[nr](a0, a1, a2, a3, a4, a5);
}
