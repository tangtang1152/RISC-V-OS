#include "sbi.h"

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

void trap_handler(unsigned long scause, unsigned long sepc, unsigned long stval) {
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

    while (1) { }
}