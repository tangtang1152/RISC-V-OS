#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"
#include "proc.h"

void trap_handler(struct trap_frame *tf) {
    unsigned long scause = r_scause();

    if (scause == 8) {   // Environment call from U-mode
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

            case SYS_YIELD: {
                int old_pid = current->pid;

                tf->sepc = r_sepc() + 4;
                tf->a0 = 0;

                if (current->state == PROC_RUNNING) {
                    current->state = PROC_RUNNABLE;
                }

                if (proc_switch() < 0) {
                    current->state = PROC_RUNNING;
                    print_str("[KERNEL] yield: no runnable proc, keep pid=");
                    print_hex((unsigned long)old_pid);
                    print_str("\n");
                } else {
                    print_str("[KERNEL] yield: pid=");
                    print_hex((unsigned long)old_pid);
                    print_str(" -> pid=");
                    print_hex((unsigned long)current->pid);
                    print_str("\n");
                }

                return;
            }

            case SYS_EXIT: {
                int old_pid = current->pid;

                print_str("[KERNEL] exit: pid=");
                print_hex((unsigned long)old_pid);
                print_str("\n");

                current->state = PROC_ZOMBIE;

                if (proc_switch() < 0) {
                    print_str("[KERNEL] no runnable proc\n");
                    proc_dump();
                    while (1) {}
                }

                print_str("[KERNEL] exit switch: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" -> pid=");
                print_hex((unsigned long)current->pid);
                print_str("\n");

                return;
            }

            default:
                print_str("[KERNEL] unknown syscall, a7=");
                print_hex(tf->a7);
                print_str("\n");
                tf->a0 = -1;
                break;
        }

        tf->sepc = r_sepc() + 4;
        return;
    }

    print_str("[KERNEL] unhandled trap, scause=");
    print_hex(scause);
    print_str(", sepc=");
    print_hex(r_sepc());
    print_str("\n");

    while (1) {}
}