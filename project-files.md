# Project Files Export

Export time: 3/31/2026, 6:02:45 AM

Source directory: `riscv`

Output file: `project-files.md`

## Directory Structure

```
riscv
├── .vscode
├── .gitignore
├── kernel.c
├── kmain.c
├── linker.ld
├── Makefile
├── riscv.h
├── sbi.c
├── sbi.h
├── start.S
├── syscall.h
├── trap.c
├── trap.h
├── trap.S
└── user.c
```

## File Statistics

- Total files: 14
- Total size: 10.3 KB

### File Type Distribution

| Extension | Files | Total Size |
| --- | --- | --- |
| .c | 5 | 4.6 KB |
| .h | 4 | 2.4 KB |
| (no extension) | 2 | 709 bytes |
| .s | 2 | 1.9 KB |
| .ld | 1 | 657 bytes |

## File Contents

### .gitignore

```plaintext
// .gitignore
kernel.elf
kernel.bin
kernel.dump
```

### kernel.c

```c
// kernel.c
#define KSTACK_SIZE 4096

__attribute__((aligned(16)))
char kernel_stack[KSTACK_SIZE];
//char *kernel_stack_top = kernel_stack + KSTACK_SIZE;
```

### kmain.c

```c
// kmain.c
#include "sbi.h"
#include "riscv.h"

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

    print_str("about to enter user mode\n");

    asm volatile("mv sp, %0" :: "r"(user_stack + USTACK_SIZE));
    asm volatile("sret");

    print_str("back in kmain?\n");

    while (1) { }
}
```

### linker.ld

```plaintext
// linker.ld
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /*
     * We link the kernel at 0x80200000 because this is a common load/entry
     * address when running a RISC-V kernel on QEMU's virt machine with OpenSBI.
     *
     * OpenSBI runs first in a more privileged mode, then transfers control
     * to the next stage (our kernel) at this address.
     */
    . = 0x80200000;

    .text : {
        *(.text.init)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(.bss.stack)
        *(COMMON)
    }
}
```

### Makefile

```plaintext
// Makefile
CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy

CFLAGS = -Wall -Wextra -O0 -g -ffreestanding -nostdlib -nostartfiles
CFLAGS += -march=rv64gc -mabi=lp64 -mcmodel=medany
LDFLAGS = -T linker.ld -nostdlib

all: kernel.bin

kernel.elf: start.S kmain.c sbi.c sbi.h linker.ld trap.S trap.c riscv.h syscall.h user.c kernel.c
	$(CC) $(CFLAGS) start.S kmain.c sbi.c trap.c trap.S user.c kernel.c $(LDFLAGS) -o kernel.elf

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

run: kernel.elf
	qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel kernel.elf

clean:
	rm -f kernel.elf kernel.bin
```

### riscv.h

```plaintext
// riscv.h
#ifndef RISCV_H
#define RISCV_H

static inline void w_stvec(void *x) {
    asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline void w_sepc(unsigned long x) {
    asm volatile("csrw sepc, %0" : : "r"(x));
}
static inline unsigned long r_sepc(void) {
    unsigned long x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

static inline void w_sstatus(unsigned long x) {
    asm volatile("csrw sstatus, %0" : : "r"(x));
}
static inline unsigned long r_sstatus(void) {
    unsigned long x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

static inline void w_sp(unsigned long x){
    asm volatile("mv sp, %0" :: "r"(x));
}
static inline unsigned long r_sp(void){
    unsigned long x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

static inline unsigned long r_scause(void) {
    unsigned long x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
}

static inline unsigned long r_stval(void) {
    unsigned long x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
}

static inline void w_sscratch(unsigned long x) {
    asm volatile("csrw sscratch, %0" : : "r"(x));
}
static inline unsigned long r_sscratch(void) {
    unsigned long x;
    asm volatile("csrr %0, sscratch" : "=r"(x));
    return x;
}

#endif
```

### sbi.c

```c
// sbi.c
#include "sbi.h"

long sbi_call(long ext, long fid, long arg0, long arg1, long arg2) {
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a6 asm("a6") = fid;
    register long a7 asm("a7") = ext;

    asm volatile (
        "ecall"
        : "+r"(a0), "+r"(a1)
        : "r"(a2), "r"(a6), "r"(a7)
        : "memory"
    );

    return a0;
}

void putchar(char c) {
    sbi_call(0x1, 0, c, 0, 0);
}

void print_str(const char *s) {
    while (*s) {
        putchar(*s++);
    }
}


void print_hex(unsigned long x) {
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
```

### sbi.h

```plaintext
// sbi.h
#ifndef SBI_H
#define SBI_H

long sbi_call(long ext, long fid, long arg0, long arg1, long arg2);
void putchar(char c);
void print_str(const char *s);
void print_hex(unsigned long x);

#endif

```

### start.S

```plaintext
// start.S
.section .text.init
.global _start

/*
 * _start is the first instruction executed in our kernel.
 *
 * At this point there is no C runtime, no stack setup from an OS,
 * and no normal program entry like main().
 *
 * So we do the minimum required work here:
 * 1. set the stack pointer
 * 2. call kmain(), our first C function
 *
 * If kmain ever returns, we stay in an infinite loop.
 */

.balign 4
_start:
    la sp, stack_top
    call kmain
    j .

.section .bss.stack
.align 12
stack:
    .space 4096
stack_top:
```

### syscall.h

```plaintext
// syscall.h
#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_PUTCHAR   1
#define SYS_PRINTSTR  2
#define SYS_GET_MAGIC 3
#define SYS_ADD       4
#define SYS_EXIT      5
#define SYS_PRINTHEX  6

long sys_putchar(char ch);
long sys_printstr(const char *s);
long sys_get_magic(void);
long sys_add(long x, long y);
long sys_exit(long code);
long sys_printhex(unsigned long x);

#endif
```

### trap.c

```c
// trap.c
#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"

void trap_handler(struct trap_frame *tf) {
    unsigned long scause = r_scause();

    if (scause == 8) {   // Environment call from U-mode
        switch (tf->a7) {
            case SYS_PUTCHAR:
                putchar((char)tf->a0);
                tf->a0 = 0;
                break;

            case SYS_PRINTSTR:
                print_str((const char *)tf->a0);
                tf->a0 = 0;
                break;

            case SYS_PRINTHEX:
                print_hex(tf->a0);
                tf->a0 = 0;
                break;

            case SYS_ADD:
                tf->a0 = tf->a0 + tf->a1;
                break;

            case SYS_GET_MAGIC:
                tf->a0 = 'Z';
                break;

            case SYS_EXIT:
                print_str("exit\n");
                while (1) {}
                break;

            default:
                print_str("[KERNEL] unknown syscall, a7=");
                print_hex(tf->a7);
                print_str("\n");
                tf->a0 = -1;
                break;
        }

        w_sepc(r_sepc() + 4);
        return;
    }

    print_str("[KERNEL] unhandled trap, scause=");
    print_hex(scause);
    print_str(", sepc=");
    print_hex(r_sepc());
    print_str("\n");

    while (1) {}
}
```

### trap.h

```plaintext
// trap.h
#ifndef TRAP_H
#define TRAP_H

struct trap_frame {
    unsigned long ra;
    unsigned long sp;

    unsigned long s0;
    unsigned long s1;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;

    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long a4;
    unsigned long a5;
    unsigned long a6;
    unsigned long a7;
};

#endif
```

### trap.S

```plaintext
// trap.S
.extern kernel_stack

.section .text
.globl kernel_entry
.globl user_entry

.balign 4
kernel_entry:
    csrw sscratch, sp
    la sp, kernel_stack
    li t0, 4096
    add sp, sp, t0

    addi sp, sp, -176

    sd ra, 0(sp)

    csrr t0, sscratch
    sd t0, 8(sp)

    sd s0, 16(sp)
    sd s1, 24(sp)
    sd s2, 32(sp)
    sd s3, 40(sp)
    sd s4, 48(sp)
    sd s5, 56(sp)
    sd s6, 64(sp)
    sd s7, 72(sp)
    sd s8, 80(sp)
    sd s9, 88(sp)
    sd s10, 96(sp)
    sd s11, 104(sp)

    sd a0, 112(sp)
    sd a1, 120(sp)
    sd a2, 128(sp)
    sd a3, 136(sp)
    sd a4, 144(sp)
    sd a5, 152(sp)
    sd a6, 160(sp)
    sd a7, 168(sp)

    mv a0, sp          # a0 = trap_frame*
    call trap_handler

    ld ra, 0(sp)

    ld s0, 16(sp)
    ld s1, 24(sp)
    ld s2, 32(sp)
    ld s3, 40(sp)
    ld s4, 48(sp)
    ld s5, 56(sp)
    ld s6, 64(sp)
    ld s7, 72(sp)
    ld s8, 80(sp)
    ld s9, 88(sp)
    ld s10, 96(sp)
    ld s11, 104(sp)

    ld a0, 112(sp)
    ld a1, 120(sp)
    ld a2, 128(sp)
    ld a3, 136(sp)
    ld a4, 144(sp)
    ld a5, 152(sp)
    ld a6, 160(sp)
    ld a7, 168(sp)

    # 最后再恢复 user sp
    ld t0, 8(sp)
    addi sp, sp, 176
    mv sp, t0

    sret

.balign 4
user_entry:
    call user_main
1:
    j 1b

.section .rodata
.balign 4
msg:
    .asciz "hello from user mode syscall\n"
```

### user.c

```c
// user.c
#include "syscall.h"

static inline unsigned long r_sp()
{
    unsigned long x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

void user_main()
{
    unsigned long sp = r_sp();

    sys_printstr("[USER] sp=");
    sys_printhex(sp);
    sys_printstr("\n");

    sys_printstr("hello\n");
}

static inline long do_syscall0(long n) {
    register long a0 asm("a0");
    register long a7 asm("a7") = n;

    asm volatile(
        "ecall"
        : "=r"(a0)
        : "r"(a7)
        : "memory"
    );

    return a0;
}
static inline long do_syscall1(long n, long x) {
    register long a0 asm("a0") = x;
    register long a7 asm("a7") = n;

    asm volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7)
        : "memory"
    );

    return a0;
}
static inline long do_syscall2(long n, long x, long y) {
    register long a0 asm("a0") = x;
    register long a1 asm("a1") = y;
    register long a7 asm("a7") = n;

    asm volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a7)
        : "memory"
    );

    return a0;
}

long sys_putchar(char ch) {
    return do_syscall1(SYS_PUTCHAR, ch);
}

long sys_printstr(const char *s) {
    return do_syscall1(SYS_PRINTSTR, (long)s);
}

long sys_get_magic(void) {
    return do_syscall0(SYS_GET_MAGIC);
}

long sys_add(long x, long y) {
    return do_syscall2(SYS_ADD, x, y);
}
                        
long sys_exit(long code) {
    return do_syscall1(SYS_EXIT, code);
}

long sys_printhex(unsigned long x) {
    return do_syscall1(SYS_PRINTHEX, x);
}
```

