#ifndef MGCORE_LIBC_SYS_STAT_H
#define MGCORE_LIBC_SYS_STAT_H

#include <mgcore/syscall.h>

int stat(const char *path, struct mgcore_stat *st);
int fstat(int fd, struct mgcore_stat *st);

#endif
