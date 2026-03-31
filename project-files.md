# Project Files Export

Export time: 3/31/2026, 9:09:34 PM

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
├── proc.c
├── proc.h
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

- Total files: 16
- Total size: 13.0 KB

### File Type Distribution

| Extension | Files | Total Size |
| --- | --- | --- |
| .c | 6 | 5.8 KB |
| .h | 5 | 3.2 KB |
| (no extension) | 2 | 747 bytes |
| .s | 2 | 2.6 KB |
| .ld | 1 | 657 bytes |

## File Contents

### .gitignore

```plaintext
// .gitignore
kernel.elf
kernel.bin
kernel.dump
project-files.md
```

### kernel.c

```c
// kernel.c

```

### kmain.c

```c
// kmain.c
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

    w_sepc(current->tf.sepc); 

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus); 

    print_str("about to enter user mode\n");

    asm volatile("mv sp, %0" :: "r"(current->tf.sp));
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

kernel.elf: start.S kmain.c sbi.c sbi.h linker.ld trap.S trap.c riscv.h syscall.h user.c kernel.c proc.c proc.h
	$(CC) $(CFLAGS) start.S kmain.c sbi.c trap.c trap.S user.c kernel.c proc.c $(LDFLAGS) -o kernel.elf

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

### proc.c

```c
// proc.c
#include "proc.h"
#include "riscv.h"
#include "sbi.h"

extern void user_entry(void);
extern void user_entry2(void);

struct proc procs[PROC_NUM];
struct proc *current = 0;

static void init_proc_context(struct proc *p, int pid, unsigned long entry) {
    p->pid = pid;
    p->state = PROC_RUNNABLE;

    p->tf.ra = 0;
    p->tf.sp = (unsigned long)(p->ustack + USTACK_SIZE);

    p->tf.a0 = 0;
    p->tf.a1 = 0;
    p->tf.a2 = 0;
    p->tf.a3 = 0;
    p->tf.a4 = 0;
    p->tf.a5 = 0;
    p->tf.a6 = 0;
    p->tf.a7 = 0;

    p->tf.sepc = entry;
}

void proc_init(void) {
    init_proc_context(&procs[0], 0, (unsigned long)user_entry);
    init_proc_context(&procs[1], 1, (unsigned long)user_entry2);

    current = &procs[0];
    current->state = PROC_RUNNING;
}
void proc_switch(void) {
    if (current->state == PROC_RUNNING) {
        current->state = PROC_RUNNABLE;
    }

    if (current == &procs[0]) {
        current = &procs[1];
    } else {
        current = &procs[0];
    }

    current->state = PROC_RUNNING;
}
```

### proc.h

```plaintext
// proc.h
#ifndef PROC_H
#define PROC_H

#include "trap.h"

#define PROC_NUM 2
#define KSTACK_SIZE 4096
#define USTACK_SIZE 4096

enum proc_state {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_ZOMBIE,
};

struct proc {
    struct trap_frame tf;
    char kstack[KSTACK_SIZE] __attribute__((aligned(16)));
    char ustack[USTACK_SIZE] __attribute__((aligned(16)));
    int state;
    int pid;
};

extern struct proc *current;
extern struct proc procs[PROC_NUM];

void proc_init(void);
void proc_switch(void);

#endif
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
#define SYS_YIELD 7

long sys_putchar(char ch);
long sys_printstr(const char *s);
long sys_get_magic(void);
long sys_add(long x, long y);
long sys_exit(long code);
long sys_printhex(unsigned long x);
long sys_yield(void);

#endif
```

### trap.c

```c
// trap.c
#include "sbi.h"
#include "riscv.h"
#include "syscall.h"
#include "trap.h"
#include "proc.h"

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
                current->state = PROC_ZOMBIE;
                while (1) {}
                break;

            case SYS_YIELD:
                tf->sepc = r_sepc() + 4;   // old proc's next pc
                tf->a0 = 0;                // old proc's return value
                proc_switch();
                return;

            default:
                print_str("[KERNEL] unknown syscall, a7=");
                print_hex(tf->a7);
                print_str("\n");
                tf->a0 = -1;
                break;
        }

        tf->sepc = r_sepc() + 4;
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

//暂不保证t0 t1 t2
struct trap_frame {
    unsigned long ra;
    unsigned long sp;
    unsigned long gp;
    unsigned long tp;

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

    unsigned long t3;
    unsigned long t4;
    unsigned long t5;
    unsigned long t6;

    unsigned long sepc;
};

#endif
```

### trap.S

```plaintext
// trap.S
.extern current

.section .text
.globl kernel_entry
.globl user_entry
.globl user_entry2

.balign 4
kernel_entry:
    # save user sp first
    csrw sscratch, sp

    # t0 = current
    la t0, current
    ld t0, 0(t0)

    # t2 = &current->tf
    mv t2, t0

    # sp = &current->kstack[KSTACK_SIZE]
    # 4328 = sizeof(trap_frame)(232) + KSTACK_SIZE(4096)
    li t1, 4328
    add sp, t0, t1

    # save user context into current->tf
    sd ra, 0(t2)

    csrr t1, sscratch
    sd t1, 8(t2)      # tf->sp = user sp

    sd gp, 16(t2)
    sd tp, 24(t2)

    sd s0, 32(t2)
    sd s1, 40(t2)
    sd s2, 48(t2)
    sd s3, 56(t2)
    sd s4, 64(t2)
    sd s5, 72(t2)
    sd s6, 80(t2)
    sd s7, 88(t2)
    sd s8, 96(t2)
    sd s9, 104(t2)
    sd s10, 112(t2)
    sd s11, 120(t2)

    sd a0, 128(t2)
    sd a1, 136(t2)
    sd a2, 144(t2)
    sd a3, 152(t2)
    sd a4, 160(t2)
    sd a5, 168(t2)
    sd a6, 176(t2)
    sd a7, 184(t2)

    sd t3, 192(t2)
    sd t4, 200(t2)
    sd t5, 208(t2)
    sd t6, 216(t2)

    csrr t1, sepc
    sd t1, 224(t2) 

    # a0 = trap_frame *
    mv a0, t2
    call trap_handler

    # t registers are caller-saved, so recompute current->tf after call
    la t0, current
    ld t0, 0(t0)
    mv t2, t0

    # restore user context from current->tf
    ld ra, 0(t2)

    ld gp, 16(t2)
    ld tp, 24(t2)

    ld s0, 32(t2)
    ld s1, 40(t2)
    ld s2, 48(t2)
    ld s3, 56(t2)
    ld s4, 64(t2)
    ld s5, 72(t2)
    ld s6, 80(t2)
    ld s7, 88(t2)
    ld s8, 96(t2)
    ld s9, 104(t2)
    ld s10, 112(t2)
    ld s11, 120(t2)

    ld a0, 128(t2)
    ld a1, 136(t2)
    ld a2, 144(t2)
    ld a3, 152(t2)
    ld a4, 160(t2)
    ld a5, 168(t2)
    ld a6, 176(t2)
    ld a7, 184(t2)

    ld t3, 192(t2)
    ld t4, 200(t2)
    ld t5, 208(t2)
    ld t6, 216(t2)

    # restore user sp last
    ld t1, 8(t2)
    mv sp, t1

    ld t1, 224(t2)
    csrw sepc, t1

    sret

.balign 4
user_entry:
    call user_main
1:
    j 1b

.balign 4
user_entry2:
    call user_main2
2:
    j 2b
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

void user_main(void)
{
    while (1) {
        sys_printstr("[USER1] hello\n");
        sys_yield();
    }
}

void user_main2(void)
{
    while (1) {
        sys_printstr("[USER2] hello\n");
        sys_yield();
    }
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
long sys_yield(void) {
    return do_syscall0(SYS_YIELD);
}
```

