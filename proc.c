#include "proc.h"
#include "riscv.h"
#include "sbi.h"
#include "memlayout.h"

extern void user_entry(void);
extern void user_entry2(void);

extern char __usertext_start[];

struct proc procs[PROC_NUM];
struct proc *current = 0;

static void init_proc_context(struct proc *p, int pid, unsigned long entry_offset) {
    p->pid = pid;
    p->state = PROC_RUNNABLE;
    p->wakeup_tick = 0;
    p->wait_pid = -1;
    p->waited_by = -1;
    p->block_reason = PROC_BLOCK_NONE;
    p->exit_code = 0;
    p->user_pagetable = vm_make_user_pagetable(pid);

    p->scratch.user_t0 = 0;
    p->scratch.user_t1 = 0;
    p->scratch.user_t2 = 0;
    p->scratch.user_sp = 0;
    p->scratch.tf_ptr = (unsigned long)&(p->tf);
    p->scratch.kstack_top = (unsigned long)(p->kstack + KSTACK_SIZE);

    p->tf.ra = 0;

    /*
     * VM-enabled stage:
     * user code still runs at its current linked high virtual address,
     * but user stack now uses a dedicated user virtual address.
     */
    p->tf.sp = USER_STACK_TOP;
    p->tf.gp = 0;
    p->tf.tp = 0;

    p->tf.t0 = 0;
    p->tf.t1 = 0;
    p->tf.t2 = 0;

    p->tf.s0 = 0;
    p->tf.s1 = 0;
    p->tf.s2 = 0;
    p->tf.s3 = 0;
    p->tf.s4 = 0;
    p->tf.s5 = 0;
    p->tf.s6 = 0;
    p->tf.s7 = 0;
    p->tf.s8 = 0;
    p->tf.s9 = 0;
    p->tf.s10 = 0;
    p->tf.s11 = 0;

    p->tf.a0 = 0;
    p->tf.a1 = 0;
    p->tf.a2 = 0;
    p->tf.a3 = 0;
    p->tf.a4 = 0;
    p->tf.a5 = 0;
    p->tf.a6 = 0;
    p->tf.a7 = 0;

    p->tf.t3 = 0;
    p->tf.t4 = 0;
    p->tf.t5 = 0;
    p->tf.t6 = 0;

    p->tf.sepc = USER_TEXT_BASE + entry_offset;
}

void proc_init(void) {
    init_proc_context(&procs[0], 0, (unsigned long)user_entry - (unsigned long)__usertext_start);
    init_proc_context(&procs[1], 1, (unsigned long)user_entry2 - (unsigned long)__usertext_start);

    current = &procs[0];
    current->state = PROC_RUNNING;
}
int proc_switch(void) {
    int start = current ? current->pid : 0;

    for (int i = 1; i <= PROC_NUM; i++) {
        int next = (start + i) % PROC_NUM;

        if (procs[next].state == PROC_RUNNABLE) {
            current = &procs[next];
            current->state = PROC_RUNNING;
            return 0;
        }
    }
    return -1;
}

void schedule(void) {
    int idle_printed = 0;

    while (1) {
        if (proc_switch() == 0) {
            return;
        }

        if (!idle_printed) {
            print_str("[KERNEL] schedule: no runnable process, wait for interrupt\n");
            idle_printed = 1;
        }

        // 关键：idle 等中断前开 SIE，否则可能永远等不到 timer
        unsigned long s = r_sstatus();
        w_sstatus(s | (1UL << 1));   // SIE=1
        // S-mode中断还是依赖U-mode的scratch来保存当前进程的上下文 
        w_sscratch((unsigned long)&current->scratch);
        asm volatile("wfi");
        w_sstatus(s);                // 恢复原值
    }
}

void proc_wakeup_sleepers(unsigned long now) {
    for (int i = 0; i < PROC_NUM; i++) {
        if (procs[i].state == PROC_BLOCKED &&
            procs[i].block_reason == PROC_BLOCK_SLEEP &&
            procs[i].wakeup_tick <= now) {
            procs[i].block_reason = PROC_BLOCK_NONE;
            procs[i].state = PROC_RUNNABLE;
        }
    }
}
void proc_wakeup_waiters(int exited_pid) {
    int waiter_pid;

    print_str("[KERNEL] wake_waiters: exited_pid=");
    print_hex((unsigned long)exited_pid);
    print_str("\n");

    if (exited_pid < 0 || exited_pid >= PROC_NUM) {
        print_str("[KERNEL] wake_waiters: invalid exited pid\n");
        return;
    }

    waiter_pid = procs[exited_pid].waited_by;

    print_str("[KERNEL] wake_waiters: waited_by=");
    print_hex((unsigned long)waiter_pid);
    print_str("\n");

    if (waiter_pid < 0 || waiter_pid >= PROC_NUM) {
        print_str("[KERNEL] wake_waiters: no valid waiter\n");
        return;
    }

    print_str("[KERNEL] wake_waiters: waiter state=");
    print_str(proc_state_name(procs[waiter_pid].state));
    print_str(" reason=");
    print_str(proc_block_reason_name(procs[waiter_pid].block_reason));
    print_str(" wait_pid=");
    print_hex((unsigned long)procs[waiter_pid].wait_pid);
    print_str("\n");

    if (procs[waiter_pid].state == PROC_BLOCKED &&
        procs[waiter_pid].block_reason == PROC_BLOCK_WAIT &&
        procs[waiter_pid].wait_pid == exited_pid) {
        procs[waiter_pid].wait_pid = -1;
        procs[waiter_pid].tf.a0 = procs[exited_pid].exit_code;
        procs[waiter_pid].block_reason = PROC_BLOCK_NONE;
        procs[waiter_pid].state = PROC_RUNNABLE;

        print_str("[KERNEL] wake_waiters: waiter -> RUNNABLE\n");
    }
}

/*
 * Reserved for future zombie resource reclamation.
 *
 * Note: this helper is intentionally not wired into SYS_WAIT yet.
 * The current kernel model supports wait-as-block/wakeup, but does
 * not yet model a true in-kernel suspended continuation that would
 * make wait+reap semantics clean at the current control-flow level.
 */
void proc_reap(int pid) {
    if (pid < 0 || pid >= PROC_NUM) {
        return;
    }

    procs[pid].state = PROC_UNUSED;
    procs[pid].block_reason = PROC_BLOCK_NONE;
    procs[pid].waited_by = -1;
    procs[pid].wait_pid = -1;
    procs[pid].wakeup_tick = 0;
}

const char *proc_state_name(int state) {
    switch (state) {
        case PROC_UNUSED:   return "UNUSED";
        case PROC_RUNNABLE: return "RUNNABLE";
        case PROC_RUNNING:  return "RUNNING";
        case PROC_BLOCKED:  return "BLOCKED";
        case PROC_ZOMBIE:   return "ZOMBIE";
        default:            return "UNKNOWN";
    }
}
const char *proc_block_reason_name(int reason) {
    switch (reason) {
        case PROC_BLOCK_NONE:  return "NONE";
        case PROC_BLOCK_SLEEP: return "SLEEP";
        case PROC_BLOCK_WAIT:  return "WAIT";
        default:               return "UNKNOWN";
    }
}

void proc_dump(void) {
    print_str("[KERNEL] proc dump begin\n");

    for (int i = 0; i < PROC_NUM; i++) {
        print_str("  pid=");
        print_hex((unsigned long)procs[i].pid);
        print_str(" state=");
        print_str(proc_state_name(procs[i].state));
        print_str(" reason=");
        print_str(proc_block_reason_name(procs[i].block_reason));
        print_str(" sepc=");
        print_hex(procs[i].tf.sepc);
        print_str(" sp=");
        print_hex(procs[i].tf.sp);
        print_str("\n");
    }

    print_str("[KERNEL] proc dump end\n");
}