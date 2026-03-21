#include "mgcore/kstring.h"
#include "mgcore/util.h"

#include <stdint.h>

#define KERNEL_HEAP_SIZE (1024 * 1024)

static uint8_t g_kernel_heap[KERNEL_HEAP_SIZE];
static size_t g_heap_offset;

size_t kalign_up(size_t value, size_t align) {
  return (value + (align - 1)) & ~(align - 1);
}

void *kalloc(size_t size) {
  void *result;
  size_t aligned = kalign_up(size, 16);
  if (aligned == 0) {
    return NULL;
  }
  if (g_heap_offset + aligned > KERNEL_HEAP_SIZE) {
    return NULL;
  }
  result = &g_kernel_heap[g_heap_offset];
  g_heap_offset += aligned;
  kmemset(result, 0, aligned);
  return result;
}

void *kcalloc(size_t count, size_t size) {
  return kalloc(count * size);
}

char *kstrdup(const char *s) {
  size_t len = kstrlen(s) + 1;
  char *copy = (char *)kalloc(len);
  if (!copy) {
    return NULL;
  }
  kmemcpy(copy, s, len);
  return copy;
}
