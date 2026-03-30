#define KSTACK_SIZE 4096

__attribute__((aligned(16)))
char kernel_stack[KSTACK_SIZE];
//char *kernel_stack_top = kernel_stack + KSTACK_SIZE;