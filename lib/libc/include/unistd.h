#ifndef MGCORE_LIBC_UNISTD_H
#define MGCORE_LIBC_UNISTD_H

#include <stddef.h>

#include "sys/types.h"

ssize_t read(int fd, void *buffer, size_t count);
ssize_t write(int fd, const void *buffer, size_t count);
int open(const char *path, int flags, int mode);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int chdir(const char *path);
char *getcwd(char *buffer, size_t size);
int mkdir(const char *path, mode_t mode);
int rmdir(const char *path);
int unlink(const char *path);
pid_t getpid(void);
void _exit(int status) __attribute__((noreturn));

#endif
