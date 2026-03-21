#ifndef MGCORE_MULTIBOOT2_H
#define MGCORE_MULTIBOOT2_H

#include <stdint.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

enum multiboot2_tag_type {
  MULTIBOOT2_TAG_END = 0,
  MULTIBOOT2_TAG_CMDLINE = 1,
  MULTIBOOT2_TAG_BOOT_LOADER_NAME = 2,
  MULTIBOOT2_TAG_MODULE = 3,
  MULTIBOOT2_TAG_BASIC_MEMINFO = 4,
  MULTIBOOT2_TAG_BOOTDEV = 5,
  MULTIBOOT2_TAG_MMAP = 6,
  MULTIBOOT2_TAG_FRAMEBUFFER = 8,
};

struct multiboot2_info {
  uint32_t total_size;
  uint32_t reserved;
};

struct multiboot2_tag {
  uint32_t type;
  uint32_t size;
};

struct multiboot2_tag_string {
  uint32_t type;
  uint32_t size;
  char string[];
};

struct multiboot2_tag_module {
  uint32_t type;
  uint32_t size;
  uint32_t mod_start;
  uint32_t mod_end;
  char cmdline[];
};

struct multiboot2_memory_map_entry {
  uint64_t addr;
  uint64_t len;
  uint32_t type;
  uint32_t zero;
};

struct multiboot2_tag_mmap {
  uint32_t type;
  uint32_t size;
  uint32_t entry_size;
  uint32_t entry_version;
  struct multiboot2_memory_map_entry entries[];
};

struct multiboot2_tag_framebuffer_common {
  uint32_t type;
  uint32_t size;
  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;
  uint16_t reserved;
};

static inline const struct multiboot2_tag *multiboot2_first_tag(const struct multiboot2_info *info) {
  return (const struct multiboot2_tag *)((const uint8_t *)info + 8);
}

static inline const struct multiboot2_tag *multiboot2_next_tag(const struct multiboot2_tag *tag) {
  uintptr_t next = (uintptr_t)tag + ((tag->size + 7U) & ~7U);
  return (const struct multiboot2_tag *)next;
}

#endif
