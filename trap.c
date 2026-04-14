#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"
#include "proc.h"
#include "timer.h"
#include "uaccess.h"

#define SCAUSE_INTERRUPT (1UL << 63)
#define SCAUSE_CODE(x)   ((x) & 0xfff)

#define SCAUSE_ECALL_U           8
#define SCAUSE_INST_PAGE_FAULT   12
#define SCAUSE_LOAD_PAGE_FAULT   13
#define SCAUSE_STORE_PAGE_FAULT  15

#define USER_STR_MAX 256

static int proc_is_zombie(int pid) {
    if (pid < 0 || pid >= PROC_NUM) {
        return 0;
    }
    return procs[pid].state == PROC_ZOMBIE;
}

static int is_user_fault(void) {
    return (r_sstatus() & (1UL << 8)) == 0; //SPP
}
static int is_page_fault(unsigned long scause) {
    return scause == SCAUSE_INST_PAGE_FAULT ||
           scause == SCAUSE_LOAD_PAGE_FAULT ||
           scause == SCAUSE_STORE_PAGE_FAULT;
}
static const char *page_fault_name(unsigned long scause) {
    switch (scause) {
        case SCAUSE_INST_PAGE_FAULT:
            return "instruction page fault";
        case SCAUSE_LOAD_PAGE_FAULT:
            return "load page fault";
        case SCAUSE_STORE_PAGE_FAULT:
            return "store page fault";
        default:
            return "unknown page fault";
    }
}

static void panic_trap(const char *prefix, unsigned long scause) {
    print_str(prefix);
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
static void kill_current_user_fault(struct trap_frame *tf, unsigned long scause) {
    int old_pid = current ? current->pid : -1;

    print_str("[KERNEL] user ");
    print_str(page_fault_name(scause));
    print_str(": pid=");
    if (current) {
        print_hex((unsigned long)current->pid);
    } else {
        print_str("none");
    }
    print_str(", scause=");
    print_hex(scause);
    print_str(", sepc=");
    print_hex(r_sepc());
    print_str(", stval=");
    print_hex(r_stval());
    print_str("\n");

    if (!current) {
        print_str("[KERNEL] no current process on user page fault\n");
        while (1) {}
    }

    current->exit_code = -1;
    current->state = PROC_ZOMBIE;
    current->block_reason = PROC_BLOCK_NONE;
    current->wait_pid = -1;
    proc_wakeup_waiters(current->pid);

    schedule();

    print_str("[KERNEL] page fault switch: pid=");
    print_hex((unsigned long)old_pid);
    print_str(" -> pid=");
    print_hex((unsigned long)current->pid);
    print_str("\n");

    vm_switch_to_user(current->user_pagetable);
}

void trap_handler(struct trap_frame *tf) {
    unsigned long scause = r_scause();

    if (scause & SCAUSE_INTERRUPT) {
        unsigned long code = SCAUSE_CODE(scause);

        if (code == 5) {   // supervisor timer interrupt
            unsigned long sstatus = r_sstatus();

            timer_tick();
            proc_wakeup_sleepers(ticks);

            if (sstatus & (1UL << 8)) {
                return;
            }

            tf->sepc = r_sepc();

            if (current->state == PROC_RUNNING) {
                current->state = PROC_RUNNABLE;
            }

            schedule();
            vm_switch_to_user(current->user_pagetable);
            return;
        }

        panic_trap("[KERNEL] unhandled timer interrupt, scause=", scause);
    }

    if (is_page_fault(scause)) {
        if (is_user_fault()) {
            kill_current_user_fault(tf, scause);
            return;
        }

        panic_trap("[KERNEL] supervisor page fault, scause=", scause);
    }

    if (scause == SCAUSE_ECALL_U) {   // Environment call from U-mode
        int advance_sepc = 1;
        int need_schedule = 0;
        int old_pid = -1;

        switch (tf->a7) {
            case SYS_PUTCHAR:
                putchar((char)tf->a0);
                tf->a0 = 0;
                break;
            
            case SYS_PRINTSTR: {
                char buf[USER_STR_MAX];

                if (copyinstr((const char *)tf->a0, buf, sizeof(buf)) < 0) {
                    tf->a0 = -1;
                    break;
                }

                print_str(buf);
                tf->a0 = 0;
                break;
            }

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
                unsigned long status_uaddr = tf->a1;

                if (target_pid < 0 || target_pid >= PROC_NUM || target_pid == current->pid) {
                    tf->a0 = -1;
                    break;
                }

                /*
                 * Allow restartable re-entry of the same wait syscall:
                 * if wait_pid/status_uaddr already match this request,
                 * treat it as the same in-flight wait instead of a new one.
                 */

                // 只有当wait_id不是-1且也不是二次进入 才报错
                if (current->wait_pid != -1 &&
                    (current->wait_pid != target_pid ||
                    current->wait_status_uaddr != status_uaddr)) {
                    tf->a0 = -1;
                    break;
                }
                if (procs[target_pid].waited_by != -1 &&
                    procs[target_pid].waited_by != current->pid) {
                    tf->a0 = -1;
                    break;
                }

                if (status_uaddr == 0) {
                    tf->a0 = -1;
                    break;
                }

                procs[target_pid].waited_by = current->pid;

                if (proc_is_zombie(target_pid)) {
                    long status = procs[target_pid].exit_code;
                    
                    if (copyout((void *)status_uaddr, &status, sizeof(status)) < 0) {
                        // clear in-flight wait state when SYS_WAIT copyout fails on zombie completion
                        current->wait_pid = -1;
                        current->wait_status_uaddr = 0;
                        tf->a0 = -1;
                        break;
                    }

                    proc_reap(target_pid);
                    current->wait_pid = -1;
                    current->wait_status_uaddr = 0;
                    tf->a0 = 0;
                    break;
                }

                current->wait_pid = target_pid;
                current->wait_status_uaddr = status_uaddr;
                current->state = PROC_BLOCKED;
                current->block_reason = PROC_BLOCK_WAIT;
                advance_sepc = 0;
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

            case SYS_FILLBUF: {
                unsigned long value = 0x1122334455667788UL;

                if (copyout((void *)tf->a0, &value, sizeof(value)) < 0) {
                    tf->a0 = -1;
                    break;
                }

                tf->a0 = 0;
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

    panic_trap("[KERNEL] unhandled trap, scause=", scause);
}