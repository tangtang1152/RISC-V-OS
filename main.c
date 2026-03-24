#include "sbi.h"

/*
 * kmain is the first C function entered by the kernel.
 *
 * At this stage, the goal is only to prove that:
 * 1. QEMU starts the machine
 * 2. OpenSBI transfers control to our kernel
 * 3. _start sets up a valid stack
 * 4. we successfully enter C code
 *
 * So for now kmain only prints a message and then stops in a loop.
 * Later this will grow into trap handling, syscalls, and user mode support.
 */
void kmain(void) {
    print_str("Hello from my RISC-V kernel!\n");

    while (1) {
        asm volatile ("wfi");
    }
}