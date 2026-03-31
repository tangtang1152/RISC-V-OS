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

            case SYS_YIELD:
                tf->sepc = r_sepc() + 4;
                tf->a0 = 0;

                if (current->state == PROC_RUNNING) {
                    current->state = PROC_RUNNABLE;
                }
                
                //没有可切换的进程，就再把state改回来接着跑
                if (proc_switch() < 0) {
                    current->state = PROC_RUNNING;
                }
                return;

            case SYS_EXIT:
                print_str("[KERNEL] proc exit pid=");
                print_hex((unsigned long)current->pid);
                print_str("\n");

                current->state = PROC_ZOMBIE;
                
                //没可切换的就进死循环等着了
                if (proc_switch() < 0) {
                    print_str("[KERNEL] no runnable proc\n");
                    while (1) {}
                }
                return;

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