#include "mgcore/syscall.h"

#include <stdint.h>

extern void syscall_entry(void);

static inline void write_msr(uint32_t msr, uint64_t value) {
  uint32_t lo = (uint32_t)value;
  uint32_t hi = (uint32_t)(value >> 32);
  __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

void syscall_arch_init(void) {
  const uint64_t star = ((uint64_t)0x18 << 48) | ((uint64_t)0x08 << 32);
  write_msr(0xC0000081, star);
  write_msr(0xC0000082, (uint64_t)(uintptr_t)syscall_entry);
  write_msr(0xC0000084, 0x200);
  write_msr(0xC0000080, 0x1);
}
