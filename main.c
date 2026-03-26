#include "sbi.h"
#include "riscv.h"

extern void kernel_entry(void);
extern void user_entry(void);

void kmain(void) {
    unsigned long sstatus;

    print_str("kmain enter\n");

    w_stvec(kernel_entry);
    print_str("stvec set\n");

    w_sepc((unsigned long)user_entry); 

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus);

    print_str("about to enter user mode\n");

    asm volatile("sret");

    print_str("back in kmain?\n");

    while (1) { }
}