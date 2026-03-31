#include "proc.h"

static struct proc init_proc;

struct proc *current = 0;

void proc_init(void) {
    current = &init_proc;
    current->state = PROC_RUNNABLE;
}