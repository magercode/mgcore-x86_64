#ifndef MGCORE_LIBC_SYS_SYSCALL_H
#define MGCORE_LIBC_SYS_SYSCALL_H

#include <stdint.h>

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_stat 4
#define SYS_fstat 5
#define SYS_lseek 8
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_munmap 11
#define SYS_brk 12
#define SYS_rt_sigaction 13
#define SYS_ioctl 16
#define SYS_sched_yield 24
#define SYS_nanosleep 35
#define SYS_getpid 39
#define SYS_signal 48
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_kill 62
#define SYS_uname 63
#define SYS_getcwd 79
#define SYS_chdir 80
#define SYS_mkdir 83
#define SYS_rmdir 84
#define SYS_unlink 87
#define SYS_gettimeofday 96
#define SYS_reboot 169
#define SYS_openat 257

intptr_t __syscall0(intptr_t nr);
intptr_t __syscall1(intptr_t nr, intptr_t a0);
intptr_t __syscall2(intptr_t nr, intptr_t a0, intptr_t a1);
intptr_t __syscall3(intptr_t nr, intptr_t a0, intptr_t a1, intptr_t a2);
intptr_t __syscall6(intptr_t nr, intptr_t a0, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5);

extern int errno;

#endif
