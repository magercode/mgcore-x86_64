#ifndef MGCORE_VMM_H
#define MGCORE_VMM_H

#include "mgcore/types.h"

#include <stdint.h>

#define MGCORE_PAGE_SIZE 4096UL
#define MGCORE_USER_BASE 0x0000000000400000ULL
#define MGCORE_USER_LIMIT 0x00007FFFFFFF0000ULL

enum vmm_prot_flags {
  VMM_PROT_READ = 1 << 0,
  VMM_PROT_WRITE = 1 << 1,
  VMM_PROT_EXEC = 1 << 2,
};

typedef struct address_space {
  uintptr_t brk_base;
  uintptr_t brk_current;
  uintptr_t next_mmap_base;
} address_space;

void vmm_init(void);
void vmm_address_space_init(address_space *space);
uintptr_t vmm_reserve_range(address_space *space, size_t length, int prot);
int vmm_map_anonymous(address_space *space, uintptr_t address, size_t length, int prot);
int vmm_unmap(address_space *space, uintptr_t address, size_t length);

#endif
