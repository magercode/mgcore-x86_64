#ifndef MGCORE_SWAP_H
#define MGCORE_SWAP_H

#include <stddef.h>

void swap_init(size_t ram_total_pages);
int swap_enable(size_t total_pages);
void swap_disable(void);
int swap_is_enabled(void);
int swap_set_used_pages(size_t used_pages);
size_t swap_total_pages(void);
size_t swap_used_pages(void);
size_t swap_free_pages(void);

#endif
