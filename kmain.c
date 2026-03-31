#include "sbi.h"
#include "riscv.h"
#include "proc.h"

extern void kernel_entry(void);
extern void user_entry(void);

void kmain(void) {
    unsigned long sstatus;

    print_str("kmain enter\n");

    proc_init();

    w_stvec(kernel_entry);
    print_str("stvec set\n");

    w_sepc((unsigned long)user_entry); 

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus); 
        
    current->state = PROC_RUNNING;

    print_str("about to enter user mode\n");

    asm volatile("mv sp, %0" :: "r"(current->ustack + USTACK_SIZE));
    asm volatile("sret");

    print_str("back in kmain?\n");

    while (1) { }
}