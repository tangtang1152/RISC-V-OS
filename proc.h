#ifndef PROC_H
#define PROC_H

#include "trap.h"
#include "vm.h"

#define PROC_NUM 2
#define KSTACK_SIZE 4096
#define USTACK_SIZE 4096

enum proc_state {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE,
};
enum proc_block_reason {
    PROC_BLOCK_NONE = 0,
    PROC_BLOCK_SLEEP,
    PROC_BLOCK_WAIT,
};

struct trap_scratch {
    unsigned long user_t1;
    unsigned long user_t2;
    unsigned long user_t0;
    unsigned long user_sp;
    unsigned long tf_ptr;
    unsigned long kstack_top;
};

struct proc {
    struct trap_scratch scratch; // 故意放第一个 sscratch 里直接放它的地址
    struct trap_frame tf;
    char kstack[KSTACK_SIZE] __attribute__((aligned(16)));
    char ustack[USTACK_SIZE] __attribute__((aligned(16)));
    int state;
    int pid;
    unsigned long wakeup_tick;
    int wait_pid;
    int waited_by;
    int block_reason;
    int exit_code;
    pagetable_t user_pagetable;
};

extern struct proc *current;
extern struct proc procs[PROC_NUM];

void proc_init(void);
int proc_switch(void);
void proc_dump(void);
void proc_wakeup_sleepers(unsigned long now);
void proc_wakeup_waiters(int exited_pid);

/* Reserved for future zombie reclamation; not used by active wait path yet. */
void proc_reap(int pid);

const char *proc_state_name(int state);
const char *proc_block_reason_name(int reason);

void schedule(void);

#endif