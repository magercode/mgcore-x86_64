#include "mgcore/console.h"
#include "mgcore/pmm.h"
#include "mgcore/safeboot.h"
#include "mgcore/sched.h"
#include "mgcore/task.h"

static uint64_t g_uptime_ticks;
static size_t g_memguard_boot_free_pages;
static size_t g_memguard_prev_free_pages;
static unsigned g_memguard_critical_streak;

void task_scheduler_tick(uint64_t now);

void scheduler_init(void) {
  g_uptime_ticks = 0;
  g_memguard_boot_free_pages = pmm_free_pages();
  g_memguard_prev_free_pages = g_memguard_boot_free_pages;
  g_memguard_critical_streak = 0;
  console_printf("sched: round-robin %u Hz slice=%u ticks\n", MGCORE_HZ, MGCORE_TIME_SLICE_TICKS);
}

static void memory_guard_tick(void) {
  size_t total = pmm_total_pages();
  size_t free = pmm_free_pages();
  pid_t suspect;
  int critical;
  int leak_signature;
  const size_t min_drop_pages = 256U;
  const unsigned min_streak = 5U;

  if (total == 0) {
    return;
  }

  /* Critical pressure: below 5% free or below 128 pages free. */
  critical = !((free * 100U > total * 5U) && (free > 128U));
  if (!critical) {
    g_memguard_critical_streak = 0;
    g_memguard_prev_free_pages = free;
    return;
  }

  ++g_memguard_critical_streak;
  leak_signature = 0;
  if (g_memguard_boot_free_pages > free + min_drop_pages) {
    leak_signature = 1;
  }
  if (g_memguard_prev_free_pages > free) {
    leak_signature = 1;
  }

  g_memguard_prev_free_pages = free;
  if (!leak_signature || g_memguard_critical_streak < min_streak) {
    if ((g_memguard_critical_streak % min_streak) == 0) {
      console_printf("memguard: critical pressure free=%u/%u pages (observing)\n",
                     (unsigned)free,
                     (unsigned)total);
    }
    return;
  }

  suspect = task_find_likely_memory_leaker();
  if (suspect > 0) {
    console_printf("memguard: free=%u/%u pages suspect pid=%u\n",
                   (unsigned)free,
                   (unsigned)total,
                   (unsigned)suspect);
    safeboot_trigger("memory leak detected", suspect);
  }

  console_printf("memguard: leak signature without suspect free=%u/%u pages\n",
                 (unsigned)free,
                 (unsigned)total);
}

void scheduler_tick(void) {
  ++g_uptime_ticks;
  task_scheduler_tick(g_uptime_ticks);
  if ((g_uptime_ticks % MGCORE_HZ) == 0) {
    memory_guard_tick();
  }
}

void scheduler_dump(void) {
  console_printf("uptime: %u ticks (%u ms)\n",
                 (unsigned)g_uptime_ticks,
                 (unsigned)scheduler_uptime_ms());
}

uint64_t scheduler_uptime_ticks(void) {
  return g_uptime_ticks;
}

uint64_t scheduler_uptime_ms(void) {
  return (g_uptime_ticks * 1000) / MGCORE_HZ;
}
