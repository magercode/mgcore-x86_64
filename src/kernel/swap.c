#include "mgcore/swap.h"

#include "mgcore/console.h"

#define SWAP_DEFAULT_DIVISOR 4U
#define SWAP_MIN_DEFAULT_PAGES 64U

static size_t g_swap_total_pages;
static size_t g_swap_used_pages;
static int g_swap_enabled;

void swap_init(size_t ram_total_pages) {
  g_swap_total_pages = ram_total_pages / SWAP_DEFAULT_DIVISOR;
  if (g_swap_total_pages == 0 && ram_total_pages > 0) {
    g_swap_total_pages = 1;
  }
  if (g_swap_total_pages < SWAP_MIN_DEFAULT_PAGES && ram_total_pages >= SWAP_MIN_DEFAULT_PAGES) {
    g_swap_total_pages = SWAP_MIN_DEFAULT_PAGES;
  }
  g_swap_used_pages = 0;
  g_swap_enabled = 0;
  console_printf("swap: initialized capacity=%u pages state=off\n", (unsigned)g_swap_total_pages);
}

int swap_enable(size_t total_pages) {
  if (total_pages != 0) {
    g_swap_total_pages = total_pages;
  }
  if (g_swap_total_pages == 0) {
    return -1;
  }
  if (g_swap_used_pages > g_swap_total_pages) {
    g_swap_used_pages = g_swap_total_pages;
  }
  g_swap_enabled = 1;
  return 0;
}

void swap_disable(void) {
  g_swap_enabled = 0;
  g_swap_used_pages = 0;
}

int swap_is_enabled(void) {
  return g_swap_enabled;
}

int swap_set_used_pages(size_t used_pages) {
  if (!g_swap_enabled) {
    return -1;
  }
  if (used_pages > g_swap_total_pages) {
    return -2;
  }
  g_swap_used_pages = used_pages;
  return 0;
}

size_t swap_total_pages(void) {
  return g_swap_total_pages;
}

size_t swap_used_pages(void) {
  return g_swap_used_pages;
}

size_t swap_free_pages(void) {
  return g_swap_total_pages - g_swap_used_pages;
}
