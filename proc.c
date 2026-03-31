#include "proc.h"
#include "riscv.h"
#include "sbi.h"

extern void user_entry(void);
extern void user_entry2(void);

struct proc procs[PROC_NUM];
struct proc *current = 0;

static void init_proc_context(struct proc *p, int pid, unsigned long entry) {
    p->pid = pid;
    p->state = PROC_RUNNABLE;

    p->tf.ra = 0;
    p->tf.sp = (unsigned long)(p->ustack + USTACK_SIZE);

    p->tf.a0 = 0;
    p->tf.a1 = 0;
    p->tf.a2 = 0;
    p->tf.a3 = 0;
    p->tf.a4 = 0;
    p->tf.a5 = 0;
    p->tf.a6 = 0;
    p->tf.a7 = 0;

    p->tf.sepc = entry;
}

void proc_init(void) {
    init_proc_context(&procs[0], 0, (unsigned long)user_entry);
    init_proc_context(&procs[1], 1, (unsigned long)user_entry2);

    current = &procs[0];
    current->state = PROC_RUNNING;
}
void proc_switch(void) {
    if (current->state == PROC_RUNNING) {
        current->state = PROC_RUNNABLE;
    }

    if (current == &procs[0]) {
        current = &procs[1];
    } else {
        current = &procs[0];
    }

    current->state = PROC_RUNNING;
}