#ifndef MGCORE_PMM_H
#define MGCORE_PMM_H

#include "mgcore/multiboot2.h"

#include <stddef.h>
#include <stdint.h>

void pmm_init(const struct multiboot2_info *info, uintptr_t kernel_start, uintptr_t kernel_end);
uintptr_t pmm_alloc_page(void);
void pmm_free_page(uintptr_t physical);
void pmm_dump(void);

#endif
