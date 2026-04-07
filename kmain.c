#include "sbi.h"
#include "riscv.h"
#include "proc.h"
#include "timer.h"
#include "vm.h"

extern void kernel_entry(void);
extern void user_return(void);

void kmain(void) {
    unsigned long sstatus;

    print_str("kmain enter\n");

    proc_init();

    w_stvec(kernel_entry);
    print_str("stvec set\n");
    
    timer_init();
    print_str("timer init done\n");

    vm_init();

    w_sscratch((unsigned long)&current->scratch);

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus); 

    print_str("about to enter user mode\n");

    user_return();

    while (1) { }
}