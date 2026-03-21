#ifndef MGCORE_TYPES_H
#define MGCORE_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef int pid_t;
typedef long ssize_t;
typedef long off_t;
typedef long time_t;
typedef unsigned int mode_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;

#define MGCORE_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#endif
