#include "sbi.h"
#include "riscv.h"

extern char kernel_stack[];
extern char stack[];
extern char stack_top[];

#define USTACK_SIZE 4096
__attribute__((aligned(16)))
char user_stack[USTACK_SIZE];

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

    //不可以在C函数中改sp 会让这个C函数栈帧无法恢复
    // w_sp((unsigned long)user_stack + USTACK_SIZE);
    //改完sp不可以执行kernel的代码 以下代码应该挪去user mode
    // unsigned long sp;
    // sp = r_sp();
    // print_str("[USER] :");
    // print_hex(sp);
    // print_str("\n");

    print_str("boot stack = ");
    print_hex((unsigned long)stack);
    print_str("\n");

    print_str("boot stack top = ");
    print_hex((unsigned long)stack_top);
    print_str("\n");

    print_str("user_stack = ");
    print_hex((unsigned long)user_stack);
    print_str("\n");

    print_str("user_stack_top = ");
    print_hex((unsigned long)user_stack + 4096);
    print_str("\n");

    print_str("kernel_stack      = ");
    print_hex((unsigned long)kernel_stack);
    print_str("\n");

    print_str("kernel_stack_top  = ");
    print_hex((unsigned long)kernel_stack + 4096);
    print_str("\n");

    print_str("about to enter user mode\n");

    asm volatile("mv sp, %0" :: "r"(user_stack + USTACK_SIZE));
    asm volatile("sret");

    print_str("back in kmain?\n");

    while (1) { }
}