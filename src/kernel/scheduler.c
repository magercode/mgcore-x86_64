#include "mgcore/console.h"
#include "mgcore/sched.h"
#include "mgcore/task.h"

static uint64_t g_uptime_ticks;

void task_scheduler_tick(uint64_t now);

void scheduler_init(void) {
  g_uptime_ticks = 0;
  console_printf("sched: round-robin %u Hz slice=%u ticks\n", MGCORE_HZ, MGCORE_TIME_SLICE_TICKS);
}

void scheduler_tick(void) {
  ++g_uptime_ticks;
  task_scheduler_tick(g_uptime_ticks);
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
