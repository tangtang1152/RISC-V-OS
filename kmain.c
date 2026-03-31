#include "sbi.h"
#include "riscv.h"
#include "proc.h"

extern void kernel_entry(void);

void kmain(void) {
    unsigned long sstatus;

    print_str("kmain enter\n");

    proc_init();

    w_stvec(kernel_entry);
    print_str("stvec set\n");
    
    timer_init();
    print_str("timer init done\n");

    w_sepc(current->tf.sepc); 

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus); 

    print_str("about to enter user mode\n");

    asm volatile("mv sp, %0" :: "r"(current->tf.sp));
    asm volatile("sret");

    while (1) { }
}