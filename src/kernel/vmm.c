#include "mgcore/console.h"
#include "mgcore/vmm.h"

void vmm_init(void) {
  console_writeln("vmm: bootstrap identity mapping active");
}

void vmm_address_space_init(address_space *space) {
  if (!space) {
    return;
  }
  space->brk_base = 0x800000;
  space->brk_current = space->brk_base;
  space->next_mmap_base = 0x20000000;
}

uintptr_t vmm_reserve_range(address_space *space, size_t length, int prot) {
  uintptr_t result;
  size_t aligned_length = (length + MGCORE_PAGE_SIZE - 1) & ~(MGCORE_PAGE_SIZE - 1);
  (void)prot;
  if (!space || aligned_length == 0) {
    return 0;
  }
  result = space->next_mmap_base;
  space->next_mmap_base += aligned_length + MGCORE_PAGE_SIZE;
  return result;
}

int vmm_map_anonymous(address_space *space, uintptr_t address, size_t length, int prot) {
  (void)space;
  (void)address;
  (void)length;
  (void)prot;
  return 0;
}

int vmm_unmap(address_space *space, uintptr_t address, size_t length) {
  (void)space;
  (void)address;
  (void)length;
  return 0;
}
