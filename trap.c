#include "sbi.h"
#include "riscv.h"

enum {
    SYS_PUTCHAR = 1,
    SYS_PRINTSTR = 2,
    SYS_GET_MAGIC = 3,
    SYS_ADD = 4,
};

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

unsigned long trap_handler(unsigned long scause, 
                           unsigned long sepc, 
                           unsigned long stval, 
                           unsigned long user_a0, 
                           unsigned long user_a1, 
                           unsigned long user_a7) {
    print_str("\n[trap]\n");

    print_str("scause = ");
    print_hex(scause);
    print_str("\n");

    print_str("sepc   = ");
    print_hex(sepc);
    print_str("\n");

    print_str("stval  = ");
    print_hex(stval);
    print_str("\n");

    // scause == 8 表示触发的 ecall
    if (scause == 8) {
        unsigned long retval = 0;

        switch (user_a7) {
            case SYS_PUTCHAR:
                putchar((char)user_a0);
                break;

            case SYS_PRINTSTR:
                print_str((const char *)user_a0);
                break;

            case SYS_GET_MAGIC:
                retval = 'Z'; // 固定返回值 'Z'
                break;

            case SYS_ADD:
                retval = user_a0 + user_a1;
                break;

            default:
                retval = (unsigned long)-1;  // 未知的 syscall
                break;
        }

        // 更新 sepc，继续执行用户态的下一条指令
        w_sepc(sepc + 4);
        
        // 返回值通过 a0 传递
        return retval;
    }

    // 如果是其他类型的 trap，进入死循环
        print_str("unexpected trap\n");
    while (1) { }
}