#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"

static void print_hex(unsigned long x) {
    char hex[] = "0123456789abcdef";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        int shift = (15 - i) * 4;
        buf[2 + i] = hex[(x >> shift) & 0xf];
    }
    buf[18] = '\0';
    print_str(buf);
}

void trap_handler(struct trap_frame *tf) {
    unsigned long scause;

    asm volatile("csrr %0, scause" : "=r"(scause));

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