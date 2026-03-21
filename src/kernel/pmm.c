#include "mgcore/console.h"
#include "mgcore/kstring.h"
#include "mgcore/pmm.h"
#include "mgcore/util.h"

#define PMM_MAX_PAGES (1024 * 1024)

static uint8_t g_bitmap[PMM_MAX_PAGES / 8];
static size_t g_total_pages;
static size_t g_free_pages;

static void set_bit(size_t page) {
  g_bitmap[page / 8] |= (uint8_t)(1u << (page % 8));
}

static void clear_bit(size_t page) {
  g_bitmap[page / 8] &= (uint8_t)~(1u << (page % 8));
}

static int test_bit(size_t page) {
  return (g_bitmap[page / 8] & (uint8_t)(1u << (page % 8))) != 0;
}

static void reserve_range(uintptr_t start, uintptr_t end) {
  size_t page;
  size_t first = start / 4096;
  size_t last = (end + 4095) / 4096;
  for (page = first; page < last && page < PMM_MAX_PAGES; ++page) {
    if (!test_bit(page)) {
      set_bit(page);
      if (g_free_pages != 0) {
        --g_free_pages;
      }
    }
  }
}

void pmm_init(const struct multiboot2_info *info, uintptr_t kernel_start, uintptr_t kernel_end) {
  const struct multiboot2_tag *tag;
  kmemset(g_bitmap, 0xFF, sizeof(g_bitmap));
  g_total_pages = 0;
  g_free_pages = 0;

  for (tag = multiboot2_first_tag(info); tag->type != MULTIBOOT2_TAG_END; tag = multiboot2_next_tag(tag)) {
    if (tag->type == MULTIBOOT2_TAG_MMAP) {
      const struct multiboot2_tag_mmap *mmap = (const struct multiboot2_tag_mmap *)tag;
      const uint8_t *cursor = (const uint8_t *)mmap->entries;
      const uint8_t *limit = (const uint8_t *)tag + tag->size;
      while (cursor < limit) {
        const struct multiboot2_memory_map_entry *entry = (const struct multiboot2_memory_map_entry *)cursor;
        if (entry->type == 1) {
          uintptr_t start = (uintptr_t)entry->addr;
          uintptr_t end = (uintptr_t)(entry->addr + entry->len);
          size_t first = start / 4096;
          size_t last = end / 4096;
          size_t page;
          if (last > PMM_MAX_PAGES) {
            last = PMM_MAX_PAGES;
          }
          for (page = first; page < last; ++page) {
            clear_bit(page);
            ++g_total_pages;
            ++g_free_pages;
          }
        }
        cursor += mmap->entry_size;
      }
    }
  }

  reserve_range(0, 0x100000);
  reserve_range(kernel_start, kernel_end);

  console_printf("pmm: total=%u pages free=%u\n", (unsigned)g_total_pages, (unsigned)g_free_pages);
}

uintptr_t pmm_alloc_page(void) {
  size_t page;
  for (page = 0; page < PMM_MAX_PAGES; ++page) {
    if (!test_bit(page)) {
      set_bit(page);
      if (g_free_pages != 0) {
        --g_free_pages;
      }
      return (uintptr_t)(page * 4096ULL);
    }
  }
  return 0;
}

void pmm_free_page(uintptr_t physical) {
  size_t page = physical / 4096;
  if (page < PMM_MAX_PAGES && test_bit(page)) {
    clear_bit(page);
    ++g_free_pages;
  }
}

void pmm_dump(void) {
  console_printf("pmm: free pages=%u\n", (unsigned)g_free_pages);
}
