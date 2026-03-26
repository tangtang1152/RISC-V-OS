#include "sbi.h"

extern void kernel_entry(void);
extern void user_entry(void);

static inline void w_stvec(void *x) {
    asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline void w_sepc(void *x) {
    asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline unsigned long r_sstatus(void) {
    unsigned long x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

static inline void w_sstatus(unsigned long x) {
    asm volatile("csrw sstatus, %0" : : "r"(x));
}

void kmain(void) {
    unsigned long sstatus;

    print_str("kmain enter\n");

    w_stvec(kernel_entry);
    print_str("stvec set\n");

    w_sepc(user_entry);

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus);

    print_str("about to enter user mode\n");

    asm volatile("sret");

    print_str("back in kmain?\n");

    while (1) { }
}