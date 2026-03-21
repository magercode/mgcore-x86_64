#ifndef MGCORE_SYSCALL_H
#define MGCORE_SYSCALL_H

#include "mgcore/signal.h"
#include "mgcore/types.h"

#include <stdint.h>

struct mgcore_utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
};

struct mgcore_timeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

struct mgcore_timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

struct mgcore_stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint64_t st_nlink;
  uint32_t st_mode;
  uint32_t st_uid;
  uint32_t st_gid;
  uint32_t __pad0;
  uint64_t st_rdev;
  int64_t st_size;
  int64_t st_blksize;
  int64_t st_blocks;
  struct mgcore_timespec st_atim;
  struct mgcore_timespec st_mtim;
  struct mgcore_timespec st_ctim;
};

typedef long (*syscall_handler_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

void syscall_init(void);
void syscall_arch_init(void);
long syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

#endif
