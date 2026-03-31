#include "timer.h"
#include "riscv.h"
#include "sbi.h"

#define TIMER_INTERVAL 1000000UL

static void timer_next(void) {
    unsigned long now = r_time();
    sbi_set_timer(now + TIMER_INTERVAL);
}

void timer_init(void) {
    unsigned long sie;
    unsigned long sstatus;

    timer_next();

    sie = r_sie();
    sie |= (1UL << 5);          // STIE Supervisor Timer Interrupt Enable
    w_sie(sie);

    sstatus = r_sstatus();
    sstatus |= (1UL << 1);      // SIE Supervisor Interrupt Enable
    w_sstatus(sstatus);
}

void timer_tick(void) {
    timer_next();
}