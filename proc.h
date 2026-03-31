#ifndef PROC_H
#define PROC_H

#include "trap.h"

#define PROC_NUM 2
#define KSTACK_SIZE 4096
#define USTACK_SIZE 4096

enum proc_state {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_ZOMBIE,
};

struct proc {
    struct trap_frame tf;
    char kstack[KSTACK_SIZE] __attribute__((aligned(16)));
    char ustack[USTACK_SIZE] __attribute__((aligned(16)));
    int state;
    int pid;
};

extern struct proc *current;
extern struct proc procs[PROC_NUM];

void proc_init(void);
int proc_switch(void);
void proc_dump(void);
const char *proc_state_name(int state);

#endif