#ifndef MGCORE_UTIL_H
#define MGCORE_UTIL_H

#include "mgcore/types.h"

#include <stddef.h>
#include <stdint.h>

void *kalloc(size_t size);
void *kcalloc(size_t count, size_t size);
char *kstrdup(const char *s);
size_t kalign_up(size_t value, size_t align);

#endif
