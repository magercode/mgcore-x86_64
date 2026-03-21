#include "unistd.h"

#include "sys/stat.h"
#include "sys/syscall.h"
#include "sys/time.h"

ssize_t read(int fd, void *buffer, size_t count) { return (ssize_t)__syscall3(SYS_read, fd, (intptr_t)buffer, (intptr_t)count); }
ssize_t write(int fd, const void *buffer, size_t count) { return (ssize_t)__syscall3(SYS_write, fd, (intptr_t)buffer, (intptr_t)count); }
int open(const char *path, int flags, int mode) { return (int)__syscall3(SYS_open, (intptr_t)path, flags, mode); }
int close(int fd) { return (int)__syscall1(SYS_close, fd); }
off_t lseek(int fd, off_t offset, int whence) { return (off_t)__syscall3(SYS_lseek, fd, offset, whence); }
int chdir(const char *path) { return (int)__syscall1(SYS_chdir, (intptr_t)path); }
char *getcwd(char *buffer, size_t size) { return (char *)(intptr_t)__syscall2(SYS_getcwd, (intptr_t)buffer, (intptr_t)size); }
int mkdir(const char *path, mode_t mode) { return (int)__syscall2(SYS_mkdir, (intptr_t)path, mode); }
int rmdir(const char *path) { return (int)__syscall1(SYS_rmdir, (intptr_t)path); }
int unlink(const char *path) { return (int)__syscall1(SYS_unlink, (intptr_t)path); }
pid_t getpid(void) { return (pid_t)__syscall0(SYS_getpid); }
int stat(const char *path, struct mgcore_stat *st) { return (int)__syscall2(SYS_stat, (intptr_t)path, (intptr_t)st); }
int fstat(int fd, struct mgcore_stat *st) { return (int)__syscall2(SYS_fstat, fd, (intptr_t)st); }
int gettimeofday(struct mgcore_timeval *tv, void *tz) { return (int)__syscall2(SYS_gettimeofday, (intptr_t)tv, (intptr_t)tz); }
int kill(pid_t pid, int signum) { return (int)__syscall2(SYS_kill, pid, signum); }
void _exit(int status) { __syscall1(SYS_exit, status); for (;;) { __asm__ volatile ("hlt"); } }
