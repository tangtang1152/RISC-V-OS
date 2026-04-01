#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned long ticks;

void timer_init(void);
void timer_tick(void);

#endif