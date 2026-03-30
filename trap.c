#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"

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

            case SYS_EXIT:
                print_str("exit\n");
                while (1) {}
                break;

            default:
                print_str("[KERNEL] unknown syscall, a7=");
                print_hex(tf->a7);
                print_str("\n");
                tf->a0 = -1;
                break;
        }

        w_sepc(r_sepc() + 4);
        return;
    }

    print_str("[KERNEL] unhandled trap, scause=");
    print_hex(scause);
    print_str(", sepc=");
    print_hex(r_sepc());
    print_str("\n");

    while (1) {}
}