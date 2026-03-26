#include "sbi.h"

extern void kernel_entry(void);

static inline void w_stvec(void *x) {
    asm volatile("csrw stvec, %0" : : "r"(x));
}

void kmain(void) {
    w_stvec(kernel_entry);
    print_str("stvec set\n");

    print_str("before ecall\n");
    asm volatile("ecall");
    print_str("after ecall\n");

    while (1) { }
}