#ifndef MGCORE_ELF_H
#define MGCORE_ELF_H

#include "mgcore/types.h"

#include <stdint.h>

typedef struct elf_image {
  const void *base;
  size_t size;
  uintptr_t entry;
} elf_image;

void elf_init(void);
int elf_parse_image(const void *data, size_t size, elf_image *out);
int elf_probe_path(const char *path, elf_image *out);

#endif
