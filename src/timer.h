#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void timer_init(uint32_t frequency);
uint32_t timer_get_ticks(void);
void timer_wait(uint32_t ticks);
void timer_enable_preemption(void);
void timer_disable_preemption(void);

#endif
