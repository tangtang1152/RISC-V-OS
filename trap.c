#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"

void trap_handler(struct trap_frame *tf) {
    unsigned long scause;
    asm volatile("csrr %0, scause" : "=r"(scause));

    // 这样的sp是kernel stack上开了trap_handler函数栈帧之后的sp
    // unsigned long sp;
    // asm volatile("mv %0, sp" : "=r"(sp));

    // print_str("[TRAP]:");
    // print_hex((unsigned long) tf);
    // print_str("\n");

    if (scause == 8) {
        switch (tf->a7) {
            case SYS_PUTCHAR:
                putchar((char)tf->a0);
                break;

            case SYS_PRINTSTR:
                print_str((const char *)tf->a0);
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
        }

        w_sepc(r_sepc() + 4);
    }
}