#include "mgcore/syscall.h"

#include "unistd.h"

#include <stddef.h>

long sys_read(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_write(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_open(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_close(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_stat(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_fstat(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_lseek(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_mmap(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_mprotect(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_munmap(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_brk(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_rt_sigaction(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_ioctl(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_sched_yield(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_nanosleep(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_getpid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_signal(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_fork(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_execve(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_exit(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_wait4(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_kill(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_uname(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_getcwd(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_chdir(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_mkdir(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_rmdir(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_unlink(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_gettimeofday(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_reboot(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
long sys_openat(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

const syscall_handler_t g_syscall_table[MGCORE_SYSCALL_MAX] = {
  [__NR_read] = sys_read,
  [__NR_write] = sys_write,
  [__NR_open] = sys_open,
  [__NR_close] = sys_close,
  [__NR_stat] = sys_stat,
  [__NR_fstat] = sys_fstat,
  [__NR_lseek] = sys_lseek,
  [__NR_mmap] = sys_mmap,
  [__NR_mprotect] = sys_mprotect,
  [__NR_munmap] = sys_munmap,
  [__NR_brk] = sys_brk,
  [__NR_rt_sigaction] = sys_rt_sigaction,
  [__NR_ioctl] = sys_ioctl,
  [__NR_sched_yield] = sys_sched_yield,
  [__NR_nanosleep] = sys_nanosleep,
  [__NR_getpid] = sys_getpid,
  [__NR_signal] = sys_signal,
  [__NR_fork] = sys_fork,
  [__NR_execve] = sys_execve,
  [__NR_exit] = sys_exit,
  [__NR_wait4] = sys_wait4,
  [__NR_kill] = sys_kill,
  [__NR_uname] = sys_uname,
  [__NR_getcwd] = sys_getcwd,
  [__NR_chdir] = sys_chdir,
  [__NR_mkdir] = sys_mkdir,
  [__NR_rmdir] = sys_rmdir,
  [__NR_unlink] = sys_unlink,
  [__NR_gettimeofday] = sys_gettimeofday,
  [__NR_reboot] = sys_reboot,
  [__NR_openat] = sys_openat,
};

const size_t g_syscall_table_count = sizeof(g_syscall_table) / sizeof(g_syscall_table[0]);
