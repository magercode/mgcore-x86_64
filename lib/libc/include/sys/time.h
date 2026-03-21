#ifndef MGCORE_LIBC_SYS_TIME_H
#define MGCORE_LIBC_SYS_TIME_H

#include <mgcore/syscall.h>

int gettimeofday(struct mgcore_timeval *tv, void *tz);

#endif
