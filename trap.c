#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"
#include "proc.h"
#include "timer.h"

#define SCAUSE_INTERRUPT (1UL << 63)
#define SCAUSE_CODE(x)   ((x) & 0xfff)

static int proc_is_zombie(int pid) {
    if (pid < 0 || pid >= PROC_NUM) {
        return 0;
    }
    return procs[pid].state == PROC_ZOMBIE;
}

void trap_handler(struct trap_frame *tf) {
    unsigned long scause = r_scause();

    if (scause & SCAUSE_INTERRUPT) {
        unsigned long code = SCAUSE_CODE(scause);

        if (code == 5) {   // supervisor timer interrupt
            timer_tick();
            proc_wakeup_sleepers(ticks);
            return;
        }

        print_str("[KERNEL] unhandled time interrupt, scause=");
        print_hex(scause);
        print_str("\n");
        proc_dump();
        while (1) {}
    }

    if (scause == 8) {   // Environment call from U-mode
        int advance_sepc = 1;
        int need_schedule = 0;
        int old_pid = -1;

        switch (tf->a7) {
            case SYS_PUTCHAR:
                putchar((char)tf->a0);
                tf->a0 = 0;
                break;
            
            case SYS_PRINTSTR:
                print_str((const char *)tf->a0);
                tf->a0 = 0;
                break;

            case SYS_PRINTHEX:
                print_hex(tf->a0);
                tf->a0 = 0;
                break;

            case SYS_ADD:
                tf->a0 = tf->a0 + tf->a1;
                break;

            case SYS_GET_MAGIC:
                tf->a0 = 'Z';
                break;

            case SYS_GETPID:
                tf->a0 = current->pid;
                break;

            case SYS_YIELD:
                old_pid = current->pid;
                tf->a0 = 0;

                if (current->state == PROC_RUNNING) 
                    current->state = PROC_RUNNABLE;

                need_schedule = 1;
                break;

            case SYS_SLEEP: {
                unsigned long n = tf->a0;

                old_pid = current->pid;
                tf->a0 = 0;

                current->wakeup_tick = ticks + n;
                current->state = PROC_BLOCKED;
                current->block_reason = PROC_BLOCK_SLEEP;

                print_str("[KERNEL] sleep: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" until tick=");
                print_hex(current->wakeup_tick);
                print_str("\n");

                need_schedule = 1;
                break;
            }

            case SYS_WAIT: {
                int target_pid = (int)tf->a0;

                if (target_pid < 0 || target_pid >= PROC_NUM || target_pid == current->pid) {
                    tf->a0 = -1;
                    break;
                }

                if (current->wait_pid != -1) {
                    tf->a0 = -1;
                    break;
                }

                if (procs[target_pid].waited_by != -1 &&
                    procs[target_pid].waited_by != current->pid) {
                    tf->a0 = -1;
                    break;
                }

                procs[target_pid].waited_by = current->pid;

                if (proc_is_zombie(target_pid)) {
                    tf->a0 = procs[target_pid].exit_code;
                    break;
                }

                current->wait_pid = target_pid;
                current->state = PROC_BLOCKED;
                current->block_reason = PROC_BLOCK_WAIT;
                need_schedule = 1;
                break;
            }

            case SYS_EXIT:{
                old_pid = current->pid;

                print_str("[KERNEL] exit: pid=");
                print_hex((unsigned long)old_pid);
                print_str("\n");

                current->exit_code = (int)tf->a0;
                current->state = PROC_ZOMBIE;
                proc_wakeup_waiters(current->pid);

                need_schedule = 1;
                break;
            }

            default:
                print_str("[KERNEL] unknown syscall, a7=");
                print_hex(tf->a7);
                print_str("\n");
                tf->a0 = -1;
                break;
            
        }

        if (advance_sepc) 
            tf->sepc = r_sepc() + 4;
        else
            tf->sepc = r_sepc();

        if (need_schedule) {
            schedule();

            if (tf->a7 == SYS_YIELD) {
                print_str("[KERNEL] yield: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" -> pid=");
                print_hex((unsigned long)current->pid);
                print_str("\n");
            } else if (tf->a7 == SYS_EXIT) {
                print_str("[KERNEL] exit switch: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" -> pid=");
                print_hex((unsigned long)current->pid);
                print_str("\n");
            }
        }
        
        vm_switch_to_user(current->user_pagetable);
        return;
    }

    print_str("[KERNEL] unhandled trap, scause=");
    print_hex(scause);
    print_str(", sepc=");
    print_hex(r_sepc());
    print_str(", stval=");
    print_hex(r_stval());
    print_str(", current pid=");
    if (current) {
        print_hex((unsigned long)current->pid);
    } else {
        print_str("none");
    }
    print_str("\n");

    proc_dump();

    while (1) {}
}