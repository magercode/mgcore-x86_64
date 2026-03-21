#ifndef MGCORE_KERNEL_H
#define MGCORE_KERNEL_H

#include "mgcore/types.h"

#include <stdint.h>

#define MGCORE_NAME "MGCORE"
#define MGCORE_VERSION "0.1.0"

void kernel_main(uint32_t multiboot_magic, uintptr_t multiboot_info_ptr);
void kernel_halt(void) __attribute__((noreturn));
void kernel_panic(const char *message) __attribute__((noreturn));

uintptr_t kernel_image_start(void);
uintptr_t kernel_image_end(void);

#endif
