#ifndef MGCORE_PMM_H
#define MGCORE_PMM_H

#include "mgcore/multiboot2.h"

#include <stddef.h>
#include <stdint.h>

#define PMM_PAGE_SIZE 4096U

void pmm_init(const struct multiboot2_info *info, uintptr_t kernel_start, uintptr_t kernel_end);
uintptr_t pmm_alloc_page(void);
void pmm_free_page(uintptr_t physical);
void pmm_dump(void);
size_t pmm_total_pages(void);
size_t pmm_free_pages(void);
size_t pmm_used_pages(void);

#endif
