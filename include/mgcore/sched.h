#ifndef MGCORE_SCHED_H
#define MGCORE_SCHED_H

#include "mgcore/types.h"

#define MGCORE_HZ 100
#define MGCORE_TIME_SLICE_TICKS 10

void scheduler_init(void);
void scheduler_tick(void);
void scheduler_dump(void);
uint64_t scheduler_uptime_ticks(void);
uint64_t scheduler_uptime_ms(void);

#endif
