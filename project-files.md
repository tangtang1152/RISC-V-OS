# Project Files Export

Export time: 4/2/2026, 12:46:10 AM

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
├── timer.c
├── timer.h
├── trap.c
├── trap.h
├── trap.S
└── user.c
```

## File Statistics

- Total files: 18
- Total size: 19.2 KB

### File Type Distribution

| Extension | Files | Total Size |
| --- | --- | --- |
| .c | 7 | 10.4 KB |
| .h | 6 | 4.1 KB |
| (no extension) | 2 | 771 bytes |
| .s | 2 | 3.3 KB |
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
#include "timer.h"

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
    w_sscratch((unsigned long)&current->scratch);

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus); 

    print_str("about to enter user mode\n");

    asm volatile("mv sp, %0" :: "r"(current->tf.sp));
    asm volatile("sret");

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

kernel.elf: start.S kmain.c sbi.c sbi.h linker.ld trap.S trap.c riscv.h syscall.h user.c kernel.c proc.c proc.h timer.h timer.c
	$(CC) $(CFLAGS) start.S kmain.c sbi.c trap.c trap.S user.c kernel.c proc.c timer.c $(LDFLAGS) -o kernel.elf

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

    p->scratch.user_t0 = 0;
    p->scratch.user_t1 = 0;
    p->scratch.user_t2 = 0;
    p->scratch.user_sp = 0;
    p->scratch.tf_ptr = (unsigned long)&(p->tf);
    p->scratch.kstack_top = (unsigned long)(p->kstack + KSTACK_SIZE);

    p->tf.ra = 0;
    p->tf.sp = (unsigned long)(p->ustack + USTACK_SIZE);
    p->tf.gp = 0;
    p->tf.tp = 0;

    p->tf.t0 = 0;
    p->tf.t1 = 0;
    p->tf.t2 = 0;

    p->tf.s0 = 0;
    p->tf.s1 = 0;
    p->tf.s2 = 0;
    p->tf.s3 = 0;
    p->tf.s4 = 0;
    p->tf.s5 = 0;
    p->tf.s6 = 0;
    p->tf.s7 = 0;
    p->tf.s8 = 0;
    p->tf.s9 = 0;
    p->tf.s10 = 0;
    p->tf.s11 = 0;

    p->tf.a0 = 0;
    p->tf.a1 = 0;
    p->tf.a2 = 0;
    p->tf.a3 = 0;
    p->tf.a4 = 0;
    p->tf.a5 = 0;
    p->tf.a6 = 0;
    p->tf.a7 = 0;

    p->tf.t3 = 0;
    p->tf.t4 = 0;
    p->tf.t5 = 0;
    p->tf.t6 = 0;

    p->tf.sepc = entry;
}

void proc_init(void) {
    init_proc_context(&procs[0], 0, (unsigned long)user_entry);
    init_proc_context(&procs[1], 1, (unsigned long)user_entry2);

    current = &procs[0];
    current->state = PROC_RUNNING;
}
int proc_switch(void) {
    int start = current ? current->pid : 0;

    for (int i = 1; i <= PROC_NUM; i++) {
        int next = (start + i) % PROC_NUM;

        if (procs[next].state == PROC_RUNNABLE) {
            current = &procs[next];
            current->state = PROC_RUNNING;
            return 0;
        }
    }

    return -1;
}

const char *proc_state_name(int state) {
    switch (state) {
        case PROC_UNUSED:   return "UNUSED";
        case PROC_RUNNABLE: return "RUNNABLE";
        case PROC_RUNNING:  return "RUNNING";
        case PROC_ZOMBIE:   return "ZOMBIE";
        default:            return "UNKNOWN";
    }
}

void proc_dump(void) {
    print_str("[KERNEL] proc dump begin\n");

    for (int i = 0; i < PROC_NUM; i++) {
        print_str("  pid=");
        print_hex((unsigned long)procs[i].pid);
        print_str(" state=");
        print_str(proc_state_name(procs[i].state));
        print_str(" sepc=");
        print_hex(procs[i].tf.sepc);
        print_str(" sp=");
        print_hex(procs[i].tf.sp);
        print_str("\n");
    }

    print_str("[KERNEL] proc dump end\n");
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

struct trap_scratch {
    unsigned long user_t1;
    unsigned long user_t2;
    unsigned long user_t0;
    unsigned long user_sp;
    unsigned long tf_ptr;
    unsigned long kstack_top;
};

struct proc {
    struct trap_scratch scratch; // 故意放第一个 sscratch 里直接放它的地址
    struct trap_frame tf;
    char kstack[KSTACK_SIZE] __attribute__((aligned(16)));
    char ustack[USTACK_SIZE] __attribute__((aligned(16)));
    int state;
    int pid;
};

extern struct proc *current;
extern struct proc procs[PROC_NUM];

void proc_init(void);
int proc_switch(void);
void proc_dump(void);
const char *proc_state_name(int state);

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

static inline void w_sie(unsigned long x) {
    asm volatile("csrw sie, %0" : : "r"(x));
}
static inline unsigned long r_sie(void) {
    unsigned long x;
    asm volatile("csrr %0, sie" : "=r"(x));
    return x;
}

static inline unsigned long r_time(void) {
    unsigned long x;
    asm volatile("csrr %0, time" : "=r"(x));
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


// 0x54 -> T 0x49 -> I 0x4D -> M 0x45 -> E
// TIME
void sbi_set_timer(unsigned long stime_value) {
    sbi_call(0x54494D45, 0, stime_value, 0, 0);
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

void sbi_set_timer(unsigned long stime_value);

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

### timer.c

```c
// timer.c
#include "timer.h"
#include "riscv.h"
#include "sbi.h"

#define TIMER_INTERVAL 1000000UL

static void timer_next(void) {
    unsigned long now = r_time();
    sbi_set_timer(now + TIMER_INTERVAL);
}

void timer_init(void) {
    unsigned long sie;
    unsigned long sstatus;

    timer_next();

    sie = r_sie();
    sie |= (1UL << 5);          // STIE Supervisor Timer Interrupt Enable
    w_sie(sie);

    sstatus = r_sstatus();
    sstatus |= (1UL << 1);      // SIE Supervisor Interrupt Enable
    w_sstatus(sstatus);
}

void timer_tick(void) {
    timer_next();
}
```

### timer.h

```plaintext
// timer.h
#ifndef TIMER_H
#define TIMER_H

void timer_init(void);
void timer_tick(void);

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
#include "timer.h"

#define SCAUSE_INTERRUPT (1UL << 63)
#define SCAUSE_CODE(x)   ((x) & 0xfff)

void trap_handler(struct trap_frame *tf) {
    unsigned long scause = r_scause();

    if (scause & SCAUSE_INTERRUPT) {
        unsigned long code = SCAUSE_CODE(scause);

        if (code == 5) {   // supervisor timer interrupt
            timer_tick();

            //和yield一样先存pc 但这次不需要加4 ecall+4是因为要跳过ecall
            tf->sepc = r_sepc(); 
            
            if (current->state == PROC_RUNNING) {
                current->state = PROC_RUNNABLE;
            }

            if (proc_switch() < 0) {
                current->state = PROC_RUNNING;
            }

            return;
        }

        print_str("[KERNEL] unhandled interrupt, scause=");
        print_hex(scause);
        print_str("\n");
        proc_dump();
        while (1) {}
    }

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

            case SYS_YIELD: {
                int old_pid = current->pid;

                tf->sepc = r_sepc() + 4;
                tf->a0 = 0;

                if (current->state == PROC_RUNNING) {
                    current->state = PROC_RUNNABLE;
                }

                if (proc_switch() < 0) {
                    current->state = PROC_RUNNING;
                    print_str("[KERNEL] yield: no runnable proc, keep pid=");
                    print_hex((unsigned long)old_pid);
                    print_str("\n");
                } else {
                    print_str("[KERNEL] yield: pid=");
                    print_hex((unsigned long)old_pid);
                    print_str(" -> pid=");
                    print_hex((unsigned long)current->pid);
                    print_str("\n");
                }

                return;
            }

            case SYS_EXIT: {
                int old_pid = current->pid;

                print_str("[KERNEL] exit: pid=");
                print_hex((unsigned long)old_pid);
                print_str("\n");

                current->state = PROC_ZOMBIE;

                if (proc_switch() < 0) {
                    print_str("[KERNEL] no runnable proc\n");
                    proc_dump();
                    while (1) {}
                }

                print_str("[KERNEL] exit switch: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" -> pid=");
                print_hex((unsigned long)current->pid);
                print_str("\n");

                return;
            }

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
    print_str(", stval=");
    print_hex(r_stval());
    print_str(", current pid=");
    if (current) {
        print_hex((unsigned long)current->pid);
    } else {
        print_str("none");
    }
    print_str("\n");

    proc_dump();

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
    unsigned long gp;
    unsigned long tp;

    unsigned long t0;
    unsigned long t1;
    unsigned long t2;
    
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
    # swap: t0 <- sscratch(scratch_ptr), sscratch <- user_t0
    csrrw t0, sscratch, t0

    # early scratch save of user t1/t2/t0/sp
    sd t1, 0(t0)              # scratch.user_t1
    sd t2, 8(t0)              # scratch.user_t2

    csrr t1, sscratch         # t1 = user_t0
    sd t1, 16(t0)             # scratch.user_t0

    sd sp, 24(t0)             # scratch.user_sp

    # load pointers prepared in proc_init()
    ld t1, 32(t0)             # scratch.tf_ptr
    ld sp, 40(t0)             # scratch.kstack_top

    # t1 = trap_frame *
    # t0 = scratch *
    sd ra, 0(t1)

    ld t2, 24(t0)
    sd t2, 8(t1)              # tf->sp = user sp

    sd gp, 16(t1)
    sd tp, 24(t1)

    ld t2, 16(t0)
    sd t2, 32(t1)             # tf->t0 = saved user_t0

    ld t2, 0(t0)
    sd t2, 40(t1)             # tf->t1 = saved user_t1

    ld t2, 8(t0)
    sd t2, 48(t1)             # tf->t2 = saved user_t2

    sd s0, 56(t1)
    sd s1, 64(t1)
    sd s2, 72(t1)
    sd s3, 80(t1)
    sd s4, 88(t1)
    sd s5, 96(t1)
    sd s6, 104(t1)
    sd s7, 112(t1)
    sd s8, 120(t1)
    sd s9, 128(t1)
    sd s10, 136(t1)
    sd s11, 144(t1)

    sd a0, 152(t1)
    sd a1, 160(t1)
    sd a2, 168(t1)
    sd a3, 176(t1)
    sd a4, 184(t1)
    sd a5, 192(t1)
    sd a6, 200(t1)
    sd a7, 208(t1)

    sd t3, 216(t1)
    sd t4, 224(t1)
    sd t5, 232(t1)
    sd t6, 240(t1)

    csrr t2, sepc
    sd t2, 248(t1)

    mv a0, t1
    call trap_handler

    # after handler, reload current scratch/tf because current may have changed
    la t0, current
    ld t0, 0(t0)              # t0 = current
    addi t0, t0, 0            # t0 = &current->scratch
    addi t1, t0, 48           # t1 = &current->tf

    # while back in user mode, sscratch must point to current scratch
    csrw sscratch, t0

    ld ra, 0(t1)

    ld gp, 16(t1)
    ld tp, 24(t1)

    ld s0, 56(t1)
    ld s1, 64(t1)
    ld s2, 72(t1)
    ld s3, 80(t1)
    ld s4, 88(t1)
    ld s5, 96(t1)
    ld s6, 104(t1)
    ld s7, 112(t1)
    ld s8, 120(t1)
    ld s9, 128(t1)
    ld s10, 136(t1)
    ld s11, 144(t1)

    ld a0, 152(t1)
    ld a1, 160(t1)
    ld a2, 168(t1)
    ld a3, 176(t1)
    ld a4, 184(t1)
    ld a5, 192(t1)
    ld a6, 200(t1)
    ld a7, 208(t1)

    ld t3, 216(t1)
    ld t4, 224(t1)
    ld t5, 232(t1)
    ld t6, 240(t1)

    ld t2, 248(t1)
    csrw sepc, t2

    ld t2, 8(t1)
    mv sp, t2                 # restore user sp

    # restore user t0/t1/t2 last
    ld t0, 32(t1)
    ld t2, 48(t1)
    ld t1, 40(t1)

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

void user_main(void)
{
    while (1) {
        sys_printstr("[USER1] hello\n");
        for (volatile int i = 0; i < 100000; i++) { }
    }
}

void user_main2(void)
{
    while (1) {
        sys_printstr("[USER2] hello\n");
        for (volatile int i = 0; i < 100000; i++) { }
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

