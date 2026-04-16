# Project Files Export

Export time: 4/14/2026, 5:48:51 PM

Source directory: `riscv`

Output file: `project-files.md`

## Directory Structure

```
riscv
├── .vscode
├── .gitignore
├── kmain.c
├── linker.ld
├── Makefile
├── memlayout.h
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
├── uaccess.c
├── uaccess.h
├── user.c
├── vm.c
└── vm.h
```

## File Statistics

- Total files: 22
- Total size: 47.8 KB

### File Type Distribution

| Extension | Files | Total Size |
| --- | --- | --- |
| .h | 9 | 7.2 KB |
| .c | 8 | 32.1 KB |
| (no extension) | 2 | 833 bytes |
| .s | 2 | 6.5 KB |
| .ld | 1 | 1.3 KB |

## File Contents

### .gitignore

```plaintext
// .gitignore
kernel.elf
kernel.bin
kernel.dump
kernel.nm
qemu-int.log
project-files.md
```

### kmain.c

```c
// kmain.c
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

    vm_init();
    proc_init();

    w_stvec(kernel_entry);
    print_str("stvec set\n");
    w_sscratch((unsigned long)&current->scratch);

    timer_init();
    print_str("timer init done\n");

    vm_switch_to_user(current->user_pagetable);

    sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);   // clear SPP -> sret returns to U-mode
    w_sstatus(sstatus); 

    print_str("about to enter user mode\n");

    user_return();

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

    . = ALIGN(4096);
    __userimage_start = .;

    __usertext_start = .;
    .usertext : {
        *(.usertext)
        *(.usertext.*)
    }
    . = ALIGN(4096);
    __usertext_end = .;

    __userrodata_start = .;
    .userrodata : {
        *(.userrodata)
        *(.userrodata.*)
    }
    . = ALIGN(4096);
    __userrodata_end = .;

    __userdata_start = .;
    .userdata : {
        *(.userdata)
        *(.userdata.*)
    }
    . = ALIGN(4096);
    __userdata_end = .;

    __userbss_start = .;
    .userbss : {
        *(.userbss)
        *(.userbss.*)
    }
    . = ALIGN(4096);
    __userbss_end = .;

    __userimage_end = .;

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

kernel.elf: start.S kmain.c sbi.c sbi.h linker.ld trap.S trap.c riscv.h syscall.h user.c proc.c proc.h timer.h timer.c vm.h vm.c memlayout.h uaccess.c uaccess.h
	$(CC) $(CFLAGS) start.S kmain.c sbi.c trap.c trap.S user.c proc.c timer.c vm.c uaccess.c $(LDFLAGS) -o kernel.elf

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

### memlayout.h

```plaintext
// memlayout.h
#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

/*
 * Intended future user virtual address layout.
 */
#define USER_BASE             0x0000000000001000UL
#define USER_TEXT_BASE        USER_BASE
#define USER_IMAGE_MAX_SIZE   0x0000000000040000UL   /* 256 KiB */
#define USER_STACK_TOP        0x0000000040000000UL
#define USER_STACK_SIZE       0x0000000000001000UL

/*
 * Shared kernel mapping window used by the first VM-enabled version.
 * We map a small high-address kernel region into every user page table,
 * supervisor-only by default, then selectively mark a user-code window
 * as user executable.
 */
#define KERNEL_MAP_BASE       0x0000000080200000UL
#define KERNEL_MAP_SIZE       0x0000000001000000UL   /* 16 MiB */
#define KERNEL_L0_SPAN        0x0000000000200000UL   /* 2 MiB per L0 table */
#define KERNEL_L0_TABLES      (KERNEL_MAP_SIZE / KERNEL_L0_SPAN) // 8

#define USER_CODE_WINDOW_SIZE 0x0000000000010000UL   /* 64 KiB */

#define PAGE_SIZE             4096UL
#define PAGE_SHIFT            12UL
#define PT_ENTRY_COUNT        512UL

#endif
```

### proc.c

```c
// proc.c
#include "proc.h"
#include "riscv.h"
#include "sbi.h"
#include "memlayout.h"

extern void user_entry(void);
extern void user_entry2(void);
extern char __usertext_start[];

struct proc procs[PROC_NUM];
struct proc *current = 0;

static void init_proc_context(struct proc *p, int pid, unsigned long entry_offset) {
    p->pid = pid;
    p->state = PROC_RUNNABLE;
    p->block_reason = PROC_BLOCK_NONE;   

    p->wakeup_tick = 0;

    p->wait_pid = -1;
    p->waited_by = -1;

    p->exit_code = 0;
    p->wait_status_uaddr = 0;

    p->user_pagetable = vm_make_user_pagetable(pid);

    p->scratch.user_t0 = 0;
    p->scratch.user_t1 = 0;
    p->scratch.user_t2 = 0;
    p->scratch.user_sp = 0;
    p->scratch.tf_ptr = (unsigned long)&(p->tf);
    p->scratch.kstack_top = (unsigned long)(p->kstack + KSTACK_SIZE);

    p->tf.ra = 0;

    /*
     * VM-enabled stage:
     * user code still runs at its current linked high virtual address,
     * but user stack now uses a dedicated user virtual address.
     */
    p->tf.sp = USER_STACK_TOP;
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

    p->tf.sepc = USER_TEXT_BASE + entry_offset;
}

void proc_init(void) {
    init_proc_context(&procs[0], 0, (unsigned long)user_entry - (unsigned long)__usertext_start);
    init_proc_context(&procs[1], 1, (unsigned long)user_entry2 - (unsigned long)__usertext_start);

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

void schedule(void) {
    int idle_printed = 0;

    while (1) {
        if (proc_switch() == 0) {
            return;
        }

        if (!idle_printed) {
            print_str("[KERNEL] schedule: no runnable process, wait for interrupt\n");
            idle_printed = 1;
        }

        // 关键：idle 等中断前开 SIE，否则可能永远等不到 timer
        unsigned long s = r_sstatus();
        w_sstatus(s | (1UL << 1));   // SIE=1
        // S-mode中断还是依赖U-mode的scratch来保存当前进程的上下文 
        w_sscratch((unsigned long)&current->scratch);
        asm volatile("wfi");
        w_sstatus(s);                // 恢复原值
    }
}

void proc_wakeup_sleepers(unsigned long now) {
    for (int i = 0; i < PROC_NUM; i++) {
        if (procs[i].state == PROC_BLOCKED &&
            procs[i].block_reason == PROC_BLOCK_SLEEP &&
            procs[i].wakeup_tick <= now) {
            procs[i].block_reason = PROC_BLOCK_NONE;
            procs[i].state = PROC_RUNNABLE;
        }
    }
}
void proc_wakeup_waiters(int exited_pid) {
    int waiter_pid;

    print_str("[KERNEL] wake_waiters: exited_pid=");
    print_hex((unsigned long)exited_pid);
    print_str("\n");

    if (exited_pid < 0 || exited_pid >= PROC_NUM) {
        print_str("[KERNEL] wake_waiters: invalid exited pid\n");
        return;
    }

    waiter_pid = procs[exited_pid].waited_by;

    print_str("[KERNEL] wake_waiters: waited_by=");
    print_hex((unsigned long)waiter_pid);
    print_str("\n");

    if (waiter_pid < 0 || waiter_pid >= PROC_NUM) {
        print_str("[KERNEL] wake_waiters: no valid waiter\n");
        return;
    }

    print_str("[KERNEL] wake_waiters: waiter state=");
    print_str(proc_state_name(procs[waiter_pid].state));
    print_str(" reason=");
    print_str(proc_block_reason_name(procs[waiter_pid].block_reason));
    print_str(" wait_pid=");
    print_hex((unsigned long)procs[waiter_pid].wait_pid);
    print_str("\n");

    if (procs[waiter_pid].state == PROC_BLOCKED &&
        procs[waiter_pid].block_reason == PROC_BLOCK_WAIT &&
        procs[waiter_pid].wait_pid == exited_pid) {
        procs[waiter_pid].block_reason = PROC_BLOCK_NONE;
        procs[waiter_pid].state = PROC_RUNNABLE;

        print_str("[KERNEL] wake_waiters: waiter -> RUNNABLE\n");
    }
}

void proc_reap(int pid) {
    if (pid < 0 || pid >= PROC_NUM) {
        return;
    }

    procs[pid].state = PROC_UNUSED;
    procs[pid].block_reason = PROC_BLOCK_NONE;
    procs[pid].waited_by = -1;
    procs[pid].wait_pid = -1;
    procs[pid].wait_status_uaddr = 0;
    procs[pid].wakeup_tick = 0;
}

const char *proc_state_name(int state) {
    switch (state) {
        case PROC_UNUSED:   return "UNUSED";
        case PROC_RUNNABLE: return "RUNNABLE";
        case PROC_RUNNING:  return "RUNNING";
        case PROC_BLOCKED:  return "BLOCKED";
        case PROC_ZOMBIE:   return "ZOMBIE";
        default:            return "UNKNOWN";
    }
}
const char *proc_block_reason_name(int reason) {
    switch (reason) {
        case PROC_BLOCK_NONE:  return "NONE";
        case PROC_BLOCK_SLEEP: return "SLEEP";
        case PROC_BLOCK_WAIT:  return "WAIT";
        default:               return "UNKNOWN";
    }
}

void proc_dump(void) {
    print_str("[KERNEL] proc dump begin\n");

    for (int i = 0; i < PROC_NUM; i++) {
        print_str("  pid=");
        print_hex((unsigned long)procs[i].pid);
        print_str(" state=");
        print_str(proc_state_name(procs[i].state));
        print_str(" reason=");
        print_str(proc_block_reason_name(procs[i].block_reason));
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
#include "vm.h"

#define PROC_NUM 2
#define KSTACK_SIZE 4096
#define USTACK_SIZE 4096

enum proc_state {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE,
};
enum proc_block_reason {
    PROC_BLOCK_NONE = 0,
    PROC_BLOCK_SLEEP,
    PROC_BLOCK_WAIT,
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
    int block_reason;

    int pid;

    unsigned long wakeup_tick;

    int wait_pid;
    int waited_by;

    int exit_code;
    unsigned long wait_status_uaddr;
    pagetable_t user_pagetable;
};

extern struct proc *current;
extern struct proc procs[PROC_NUM];

void proc_init(void);
int proc_switch(void);
void proc_dump(void);
void proc_wakeup_sleepers(unsigned long now);
void proc_wakeup_waiters(int exited_pid);

/* Reserved for future zombie reclamation; not used by active wait path yet. */
void proc_reap(int pid);

const char *proc_state_name(int state);
const char *proc_block_reason_name(int reason);

void schedule(void);

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

static inline void w_satp(unsigned long x) {
    asm volatile("csrw satp, %0" : : "r"(x));
}

static inline unsigned long r_satp(void) {
    unsigned long x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

static inline void sfence_vma(void) {
    asm volatile("sfence.vma zero, zero");
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
#define SYS_YIELD     7
#define SYS_SLEEP     8
#define SYS_GETPID    9
#define SYS_WAIT      10
#define SYS_FILLBUF   11

long sys_putchar(char ch);
long sys_printstr(const char *s);
long sys_get_magic(void);
long sys_add(long x, long y);
long sys_exit(long code);
long sys_printhex(unsigned long x);
long sys_yield(void);
long sys_sleep(long tick_count);
long sys_wait(long pid, long *status);
long sys_getpid(void);
long sys_fillbuf(unsigned long *buf);

#endif
```

### timer.c

```c
// timer.c
#include "timer.h"
#include "riscv.h"
#include "sbi.h"

#define TIMER_INTERVAL 1000000UL

volatile unsigned long ticks = 0;

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
    ticks++;
    timer_next();
}
```

### timer.h

```plaintext
// timer.h
#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned long ticks;

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
#include "uaccess.h"

#define SCAUSE_INTERRUPT (1UL << 63)
#define SCAUSE_CODE(x)   ((x) & 0xfff)

#define SCAUSE_ECALL_U           8
#define SCAUSE_INST_PAGE_FAULT   12
#define SCAUSE_LOAD_PAGE_FAULT   13
#define SCAUSE_STORE_PAGE_FAULT  15

#define USER_STR_MAX 256

static int proc_is_zombie(int pid) {
    if (pid < 0 || pid >= PROC_NUM) {
        return 0;
    }
    return procs[pid].state == PROC_ZOMBIE;
}

static int is_user_fault(void) {
    return (r_sstatus() & (1UL << 8)) == 0; //SPP
}
static int is_page_fault(unsigned long scause) {
    return scause == SCAUSE_INST_PAGE_FAULT ||
           scause == SCAUSE_LOAD_PAGE_FAULT ||
           scause == SCAUSE_STORE_PAGE_FAULT;
}
static const char *page_fault_name(unsigned long scause) {
    switch (scause) {
        case SCAUSE_INST_PAGE_FAULT:
            return "instruction page fault";
        case SCAUSE_LOAD_PAGE_FAULT:
            return "load page fault";
        case SCAUSE_STORE_PAGE_FAULT:
            return "store page fault";
        default:
            return "unknown page fault";
    }
}

static void panic_trap(const char *prefix, unsigned long scause) {
    print_str(prefix);
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
static void kill_current_user_fault(struct trap_frame *tf, unsigned long scause) {
    int old_pid = current ? current->pid : -1;

    print_str("[KERNEL] user ");
    print_str(page_fault_name(scause));
    print_str(": pid=");
    if (current) {
        print_hex((unsigned long)current->pid);
    } else {
        print_str("none");
    }
    print_str(", scause=");
    print_hex(scause);
    print_str(", sepc=");
    print_hex(r_sepc());
    print_str(", stval=");
    print_hex(r_stval());
    print_str("\n");

    if (!current) {
        print_str("[KERNEL] no current process on user page fault\n");
        while (1) {}
    }

    current->exit_code = -1;
    current->state = PROC_ZOMBIE;
    current->block_reason = PROC_BLOCK_NONE;
    current->wait_pid = -1;
    proc_wakeup_waiters(current->pid);

    schedule();

    print_str("[KERNEL] page fault switch: pid=");
    print_hex((unsigned long)old_pid);
    print_str(" -> pid=");
    print_hex((unsigned long)current->pid);
    print_str("\n");

    vm_switch_to_user(current->user_pagetable);
}

void trap_handler(struct trap_frame *tf) {
    unsigned long scause = r_scause();

    if (scause & SCAUSE_INTERRUPT) {
        unsigned long code = SCAUSE_CODE(scause);

        if (code == 5) {   // supervisor timer interrupt
            unsigned long sstatus = r_sstatus();

            timer_tick();
            proc_wakeup_sleepers(ticks);

            if (sstatus & (1UL << 8)) {
                return;
            }

            tf->sepc = r_sepc();

            if (current->state == PROC_RUNNING) {
                current->state = PROC_RUNNABLE;
            }

            schedule();
            vm_switch_to_user(current->user_pagetable);
            return;
        }

        panic_trap("[KERNEL] unhandled timer interrupt, scause=", scause);
    }

    if (is_page_fault(scause)) {
        if (is_user_fault()) {
            kill_current_user_fault(tf, scause);
            return;
        }

        panic_trap("[KERNEL] supervisor page fault, scause=", scause);
    }

    if (scause == SCAUSE_ECALL_U) {   // Environment call from U-mode
        int advance_sepc = 1;
        int need_schedule = 0;
        int old_pid = -1;

        switch (tf->a7) {
            case SYS_PUTCHAR:
                putchar((char)tf->a0);
                tf->a0 = 0;
                break;
            
            case SYS_PRINTSTR: {
                char buf[USER_STR_MAX];

                if (copyinstr((const char *)tf->a0, buf, sizeof(buf)) < 0) {
                    tf->a0 = -1;
                    break;
                }

                print_str(buf);
                tf->a0 = 0;
                break;
            }

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

            case SYS_GETPID:
                tf->a0 = current->pid;
                break;

            case SYS_YIELD:
                old_pid = current->pid;
                tf->a0 = 0;

                if (current->state == PROC_RUNNING) 
                    current->state = PROC_RUNNABLE;

                need_schedule = 1;
                break;

            case SYS_SLEEP: {
                unsigned long n = tf->a0;

                old_pid = current->pid;
                tf->a0 = 0;

                current->wakeup_tick = ticks + n;
                current->state = PROC_BLOCKED;
                current->block_reason = PROC_BLOCK_SLEEP;

                print_str("[KERNEL] sleep: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" until tick=");
                print_hex(current->wakeup_tick);
                print_str("\n");

                need_schedule = 1;
                break;
            }

            case SYS_WAIT: {
                int target_pid = (int)tf->a0;
                unsigned long status_uaddr = tf->a1;

                if (target_pid < 0 || target_pid >= PROC_NUM || target_pid == current->pid) {
                    tf->a0 = -1;
                    break;
                }

                /*
                 * Allow restartable re-entry of the same wait syscall:
                 * if wait_pid/status_uaddr already match this request,
                 * treat it as the same in-flight wait instead of a new one.
                 */

                // 只有当wait_id不是-1且也不是二次进入 才报错
                if (current->wait_pid != -1 &&
                    (current->wait_pid != target_pid ||
                    current->wait_status_uaddr != status_uaddr)) {
                    tf->a0 = -1;
                    break;
                }
                if (procs[target_pid].waited_by != -1 &&
                    procs[target_pid].waited_by != current->pid) {
                    tf->a0 = -1;
                    break;
                }

                if (status_uaddr == 0) {
                    tf->a0 = -1;
                    break;
                }

                procs[target_pid].waited_by = current->pid;

                if (proc_is_zombie(target_pid)) {
                    long status = procs[target_pid].exit_code;
                    
                    if (copyout((void *)status_uaddr, &status, sizeof(status)) < 0) {
                        // clear in-flight wait state when SYS_WAIT copyout fails on zombie completion
                        current->wait_pid = -1;
                        current->wait_status_uaddr = 0;
                        tf->a0 = -1;
                        break;
                    }

                    proc_reap(target_pid);
                    current->wait_pid = -1;
                    current->wait_status_uaddr = 0;
                    tf->a0 = 0;
                    break;
                }

                current->wait_pid = target_pid;
                current->wait_status_uaddr = status_uaddr;
                current->state = PROC_BLOCKED;
                current->block_reason = PROC_BLOCK_WAIT;
                advance_sepc = 0;
                need_schedule = 1;
                break;
            }

            case SYS_EXIT:{
                old_pid = current->pid;

                print_str("[KERNEL] exit: pid=");
                print_hex((unsigned long)old_pid);
                print_str("\n");

                current->exit_code = (int)tf->a0;
                current->state = PROC_ZOMBIE;
                proc_wakeup_waiters(current->pid);

                need_schedule = 1;
                break;
            }

            case SYS_FILLBUF: {
                unsigned long value = 0x1122334455667788UL;

                if (copyout((void *)tf->a0, &value, sizeof(value)) < 0) {
                    tf->a0 = -1;
                    break;
                }

                tf->a0 = 0;
                break;
            }
            
            default:
                print_str("[KERNEL] unknown syscall, a7=");
                print_hex(tf->a7);
                print_str("\n");
                tf->a0 = -1;
                break;
            
        }

        if (advance_sepc) 
            tf->sepc = r_sepc() + 4;
        else
            tf->sepc = r_sepc();

        if (need_schedule) {
            schedule();

            if (tf->a7 == SYS_YIELD) {
                print_str("[KERNEL] yield: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" -> pid=");
                print_hex((unsigned long)current->pid);
                print_str("\n");
            } else if (tf->a7 == SYS_EXIT) {
                print_str("[KERNEL] exit switch: pid=");
                print_hex((unsigned long)old_pid);
                print_str(" -> pid=");
                print_hex((unsigned long)current->pid);
                print_str("\n");
            }
        }
        
        vm_switch_to_user(current->user_pagetable);
        return;
    }

    panic_trap("[KERNEL] unhandled trap, scause=", scause);
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
.globl user_trap_entry
.globl user_entry
.globl user_entry2
.global user_return
.global kernel_return

.balign 4
user_trap_entry:
    # Trap came from U-mode.
    #
    # sscratch convention while user is running: sscratch里面尽量一直放&current->scratch
    #   sscratch = &current->scratch
    #
    # After csrrw:
    #   t0 = &current->scratch
    #   sscratch = user t0
    csrrw t0, sscratch, t0

    # Save user temporaries that would otherwise be clobbered early.
    sd t1, 0(t0)              # scratch.user_t1
    sd t2, 8(t0)              # scratch.user_t2

    # Save original user t0, then restore sscratch ASAP so nested S-traps
    # will still see a valid scratch pointer if they ever need it later.
    csrr t1, sscratch
    sd t1, 16(t0)             # scratch.user_t0
    csrw sscratch, t0

    # Save user sp into scratch before switching stacks.
    sd sp, 24(t0)             # scratch.user_sp

    # Load trapframe pointer and kernel stack top prepared by proc_init().
    ld t1, 32(t0)             # scratch.tf_ptr
    ld sp, 40(t0)             # scratch.kstack_top

    # We are now running in kernel context on kernel stack.
    # Any nested trap from here on must go directly to kernel_entry.
    la t2, kernel_entry
    csrw stvec, t2

    # Save user register context into trapframe.
    sd ra, 0(t1)

    ld t2, 24(t0)
    sd t2, 8(t1)              # tf->sp = saved user sp

    sd gp, 16(t1)
    sd tp, 24(t1)

    ld t2, 16(t0)
    sd t2, 32(t1)             # tf->t0 = saved user t0

    ld t2, 0(t0)
    sd t2, 40(t1)             # tf->t1 = saved user t1

    ld t2, 8(t0)
    sd t2, 48(t1)             # tf->t2 = saved user t2

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
    j user_return


.balign 4
kernel_entry:
    # Trap came from S-mode.
    #
    # Do NOT depend on sscratch or user-trap conventions here.
    # Just build a temporary trapframe directly on the current kernel stack.
    addi sp, sp, -256

    sd ra, 0(sp)

    # Save interrupted kernel sp = current sp + 256
    addi t0, sp, 256
    sd t0, 8(sp)

    sd gp, 16(sp)
    sd tp, 24(sp)

    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)

    sd s0, 56(sp)
    sd s1, 64(sp)
    sd s2, 72(sp)
    sd s3, 80(sp)
    sd s4, 88(sp)
    sd s5, 96(sp)
    sd s6, 104(sp)
    sd s7, 112(sp)
    sd s8, 120(sp)
    sd s9, 128(sp)
    sd s10, 136(sp)
    sd s11, 144(sp)

    sd a0, 152(sp)
    sd a1, 160(sp)
    sd a2, 168(sp)
    sd a3, 176(sp)
    sd a4, 184(sp)
    sd a5, 192(sp)
    sd a6, 200(sp)
    sd a7, 208(sp)

    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    csrr t0, sepc
    sd t0, 248(sp)

    # 把这块临时 trapframe 的地址传给 trap_handler
    # S-trap 不再关心 current->scratch，也不关心 sscratch 里有什么。
    mv a0, sp
    call trap_handler

    j kernel_return


.balign 4
user_return:
    # trap_handler may have switched current, so reload everything from current.
    la t0, current
    ld t0, 0(t0)              # t0 = current
    addi t1, t0, 48           # t1 = &current->tf   (scratch is first field)

    # While user runs:
    #   sscratch = &current->scratch
    csrw sscratch, t0

    # While user runs:
    #   traps must enter through user_trap_entry
    la t2, user_trap_entry
    csrw stvec, t2

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

    # Restore user t0/t1/t2 last.
    ld t0, 32(t1)
    ld t2, 48(t1)
    ld t1, 40(t1)

    sret


.balign 4
kernel_return:
    # Restore interrupted kernel context from temporary stack trapframe.

    ld t0, 248(sp)
    csrw sepc, t0

    # Keep sscratch pointing at current->scratch for the next user trap.
    # 虽然当前是从 S-trap 返回 S-mode，但你之后迟早还会回到用户态。
    la t3, current
    ld t3, 0(t3)
    csrw sscratch, t3

    # While staying in kernel:
    #   traps must enter through kernel_entry
    la t4, kernel_entry
    csrw stvec, t4

    ld ra, 0(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)

    ld s0, 56(sp)
    ld s1, 64(sp)
    ld s2, 72(sp)
    ld s3, 80(sp)
    ld s4, 88(sp)
    ld s5, 96(sp)
    ld s6, 104(sp)
    ld s7, 112(sp)
    ld s8, 120(sp)
    ld s9, 128(sp)
    ld s10, 136(sp)
    ld s11, 144(sp)

    ld a0, 152(sp)
    ld a1, 160(sp)
    ld a2, 168(sp)
    ld a3, 176(sp)
    ld a4, 184(sp)
    ld a5, 192(sp)
    ld a6, 200(sp)
    ld a7, 208(sp)

    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)

    # Restore t0/t1/t2 last.
    ld t2, 48(sp)
    ld t1, 40(sp)
    ld t0, 32(sp)

    addi sp, sp, 256
    sret


.section .usertext
.balign 4096
.globl user_entry
.globl user_entry2

user_entry:
    call user_main
1:
    j 1b

.balign 4096
user_entry2:
    call user_main2
2:
    j 2b
```

### uaccess.c

```c
// uaccess.c
#include "uaccess.h"
#include "riscv.h"
#include "memlayout.h"

#define SSTATUS_SUM (1UL << 18)

static int range_in_segment(unsigned long addr, unsigned long len, unsigned long start, unsigned long end) {
    unsigned long last;

    if (len == 0) {
        return 1;
    }

    if (addr < start || addr >= end) {
        return 0;
    }

    last = addr + len - 1;
    if (last < addr) {
        return 0;
    }

    return last < end;
}

static int user_range_ok(const void *uaddr, unsigned long len) {
    unsigned long addr = (unsigned long)uaddr;
    unsigned long user_img_start = USER_TEXT_BASE;
    unsigned long user_img_end = USER_TEXT_BASE + USER_IMAGE_MAX_SIZE;
    unsigned long user_stack_start = USER_STACK_TOP - USER_STACK_SIZE;
    unsigned long user_stack_end = USER_STACK_TOP;

    return range_in_segment(addr, len, user_img_start, user_img_end) ||
           range_in_segment(addr, len, user_stack_start, user_stack_end);
}

int copyin(const void *uaddr, void *kaddr, unsigned long len) {
    const unsigned char *src = (const unsigned char *)uaddr;
    unsigned char *dst = (unsigned char *)kaddr;
    unsigned long sstatus;

    if ((!uaddr && len != 0) || (!kaddr && len != 0)) {
        return -1;
    }
    if (!user_range_ok(uaddr, len)) {
        return -1;
    }

    sstatus = r_sstatus();
    w_sstatus(sstatus | SSTATUS_SUM);

    for (unsigned long i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    w_sstatus(sstatus);
    return 0;
}

int copyout(void *uaddr, const void *kaddr, unsigned long len) {
    unsigned char *dst = (unsigned char *)uaddr;
    const unsigned char *src = (const unsigned char *)kaddr;
    unsigned long sstatus;

    if ((!uaddr && len != 0) || (!kaddr && len != 0)) {
        return -1;
    }
    if (!user_range_ok(uaddr, len)) {
        return -1;
    }

    sstatus = r_sstatus();
    w_sstatus(sstatus | SSTATUS_SUM);

    for (unsigned long i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    w_sstatus(sstatus);
    return 0;
}

int copyinstr(const char *uaddr, char *kbuf, unsigned long maxlen) {
    unsigned long sstatus;

    if (!uaddr || !kbuf || maxlen == 0) {
        return -1;
    }

    sstatus = r_sstatus();
    w_sstatus(sstatus | SSTATUS_SUM);

    for (unsigned long i = 0; i < maxlen; i++) {
        if (!user_range_ok(uaddr + i, 1)) {
            w_sstatus(sstatus);
            return -1;
        }

        char ch = uaddr[i];
        kbuf[i] = ch;
        if (ch == '\0') {
            w_sstatus(sstatus);
            return 0;
        }
    }

    w_sstatus(sstatus);
    kbuf[maxlen - 1] = '\0';
    return -1;
}

```

### uaccess.h

```plaintext
// uaccess.h
#ifndef UACCESS_H
#define UACCESS_H

int copyin(const void *uaddr, void *kaddr, unsigned long len);
int copyout(void *uaddr, const void *kaddr, unsigned long len);
int copyinstr(const char *uaddr, char *kbuf, unsigned long maxlen);

#endif
```

### user.c

```c
// user.c
#define USER_TEXT    __attribute__((section(".usertext")))
#define USER_RODATA  __attribute__((section(".userrodata")))
#define USER_DATA    __attribute__((section(".userdata")))
#define USER_BSS     __attribute__((section(".userbss")))
#include "syscall.h"

static const char u0_pid[] USER_RODATA = "[USER0] pid=";
static const char u0_magic[] USER_RODATA = " magic=";
static const char u0_add[] USER_RODATA = " add(20,22) + user_data_value + user_bss_value =";
static const char u0_nl[] USER_RODATA = "\n";
static const char u0_wait[] USER_RODATA = "[USER0] wait pid=1\n";
static const char u0_wait_ret[] USER_RODATA = "[USER0] wait returned, exit code=";
static const char u0_pass[] USER_RODATA = "[USER0] TEST PASS\n";
static const char u0_fail[] USER_RODATA = "[USER0] TEST FAIL\n";

static const char u1_pid[] USER_RODATA = "[USER1] pid=";
static const char u1_add[] USER_RODATA = " add(10,32)=";
static const char u1_sleep[] USER_RODATA = "[USER1] sleep 3 ticks\n";
static const char u1_exit[] USER_RODATA = "[USER1] exit now\n";

static const char u0_copyout[] USER_RODATA = " copyout=";
static const char u0_copyout_fail[] USER_RODATA = "[USER0] copyout failed\n";

//test
static unsigned long u0_expected_copyout USER_DATA = 0x1122334455667788UL;
static long user_data_value USER_DATA = 7;
static long user_bss_value USER_BSS;

static unsigned long user_copyout_value USER_BSS;

USER_TEXT void user_main(void)
{
    long pid = sys_getpid();
    long magic = sys_get_magic();
    long sum = sys_add(20, 22);

    user_bss_value += 35;
    sum = sys_add(sum, user_data_value);
    sum = sys_add(sum, user_bss_value);

    if (sys_fillbuf(&user_copyout_value) == 0) {
        sys_printstr(u0_pid);
        sys_printhex((unsigned long)pid);
        sys_printstr(u0_magic);
        sys_printhex((unsigned long)magic);
        sys_printstr(u0_add);
        sys_printhex((unsigned long)sum);
        sys_printstr(u0_copyout);
        sys_printhex(user_copyout_value);
        sys_printstr(u0_nl);
    } else {
        sys_printstr(u0_copyout_fail);
    }

    long code = -1;
    long status = -1;

    sys_printstr(u0_wait);
    code = sys_wait(1, &status);
    sys_printstr(u0_wait_ret);
    sys_printhex((unsigned long)status);
    sys_printstr(u0_nl);

    if (code == 0 &&
        status == 42 &&
        sum == 84 &&
        magic == 'Z' &&
        user_copyout_value == u0_expected_copyout) {
        sys_printstr(u0_pass);
    } else {
        sys_printstr(u0_fail);
    }

    sys_exit(0);
    while (1) { }
}

USER_TEXT void user_main2(void){
    long pid = sys_getpid();
    long sum = sys_add(10, 32);

    sys_printstr(u1_pid);
    sys_printhex((unsigned long)pid);
    sys_printstr(u1_add);
    sys_printhex((unsigned long)sum);
    sys_printstr(u0_nl);

    sys_printstr(u1_sleep);
    sys_sleep(3);
    sys_yield();

    sys_printstr(u1_exit);
    sys_exit(42);

    while (1) { }
}

USER_TEXT static inline long do_syscall0(long n) {
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
USER_TEXT static inline long do_syscall1(long n, long x) {
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
USER_TEXT static inline long do_syscall2(long n, long x, long y) {
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

USER_TEXT long sys_putchar(char ch) {
    return do_syscall1(SYS_PUTCHAR, ch);
}

USER_TEXT long sys_printstr(const char *s) {
    return do_syscall1(SYS_PRINTSTR, (long)s);
}

USER_TEXT long sys_get_magic(void) {
    return do_syscall0(SYS_GET_MAGIC);
}

USER_TEXT long sys_add(long x, long y) {
    return do_syscall2(SYS_ADD, x, y);
}

USER_TEXT long sys_exit(long code) {
    return do_syscall1(SYS_EXIT, code);
}

USER_TEXT long sys_printhex(unsigned long x) {
    return do_syscall1(SYS_PRINTHEX, x);
}

USER_TEXT long sys_yield(void) {
    return do_syscall0(SYS_YIELD);
}

USER_TEXT long sys_sleep(long tick_count) {
    return do_syscall1(SYS_SLEEP, tick_count);
}

USER_TEXT long sys_wait(long pid, long *status) {
    return do_syscall2(SYS_WAIT, pid, (long)status);
}

USER_TEXT long sys_getpid(void) {
    return do_syscall0(SYS_GETPID);
}

USER_TEXT long sys_fillbuf(unsigned long *buf) {
    return do_syscall1(SYS_FILLBUF, (long)buf);
}

```

### vm.c

```c
// vm.c
#include "vm.h"
#include "proc.h"
#include "sbi.h"
#include "riscv.h"
#include "memlayout.h"

extern char __userimage_start[];
extern char __userimage_end[];
extern char __usertext_start[];
extern char __usertext_end[];
extern char __userrodata_start[];
extern char __userrodata_end[];
extern char __userdata_start[];
extern char __userdata_end[];
extern char __userbss_start[];
extern char __userbss_end[];

#define USER_PT_PAGE_COUNT (5 + KERNEL_L0_TABLES)

static pte_t user_pts[PROC_NUM][USER_PT_PAGE_COUNT][PT_ENTRY_COUNT]
    __attribute__((aligned(PAGE_SIZE)));

static unsigned char user_image_pages[PROC_NUM][USER_IMAGE_MAX_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

static unsigned char user_stack_pages[PROC_NUM][USER_STACK_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

/*
 * Layout per process:
 *   [0] root L2
 *   [1] low L1 (for low user region)
 *   [2] low L0 (user image leaf table)
 *   [3] low L0 (stack leaf table)
 *   [4] kernel L1
 *   [5..] kernel L0 tables covering KERNEL_MAP_SIZE
 */

static inline unsigned long vpn2(unsigned long va) {
    return (va >> 30) & 0x1ffUL;
}

static inline unsigned long vpn1(unsigned long va) {
    return (va >> 21) & 0x1ffUL;
}

static inline unsigned long vpn0(unsigned long va) {
    return (va >> 12) & 0x1ffUL;
}

static inline unsigned long page_down(unsigned long x) {
    return x & ~(PAGE_SIZE - 1);
}

static inline unsigned long page_up(unsigned long x) {
    return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static inline unsigned long pa_to_pte(unsigned long pa) {
    return ((pa >> PAGE_SHIFT) << 10);
}

static inline unsigned long pte_to_pa(pte_t pte) {
    return ((pte >> 10) << PAGE_SHIFT);
}

void vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long perm) {
    pte_t *l2 = (pte_t *)pt;
    pte_t *l1;
    pte_t *l0;
    unsigned long leaf_perm = perm | PTE_A;

    if (perm & PTE_W) {
        leaf_perm |= PTE_D;
    }

    if (!(l2[vpn2(va)] & PTE_V)) {
        print_str("[KERNEL] vm_map_page: missing l1 table\n");
        return;
    }
    l1 = (pte_t *)pte_to_pa(l2[vpn2(va)]);

    if (!(l1[vpn1(va)] & PTE_V)) {
        print_str("[KERNEL] vm_map_page: missing l0 table\n");
        return;
    }
    l0 = (pte_t *)pte_to_pa(l1[vpn1(va)]);

    l0[vpn0(va)] = pa_to_pte(pa) | leaf_perm | PTE_V;
}

unsigned long vm_make_satp(pagetable_t pt) {
    return SATP_MODE_SV39 | (pt >> PAGE_SHIFT);
}

void vm_switch_to_user(pagetable_t pt) {
    if (!pt) {
        return;
    }

    w_satp(vm_make_satp(pt));
    sfence_vma();
}

void vm_init(void) {
    for (unsigned long p = 0; p < PROC_NUM; p++) {
        for (unsigned long table = 0; table < USER_PT_PAGE_COUNT; table++) {
            for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
                user_pts[p][table][i] = 0;
            }
        }

        for (unsigned long i = 0; i < USER_IMAGE_MAX_SIZE; i++) {
            user_image_pages[p][i] = 0;
        }

        for (unsigned long i = 0; i < USER_STACK_SIZE; i++) {
            user_stack_pages[p][i] = 0;
        }
    }

    print_str("vm scaffold init done\n");
}

pagetable_t vm_make_user_pagetable(int pid) {
    pte_t *l2;
    pte_t *l1_low;
    pte_t *l0_image;
    pte_t *l0_stack;
    pte_t *l1_kernel;
    pte_t *l0_kernel[KERNEL_L0_TABLES];
    unsigned long stack_bottom;
    unsigned long image_size;
    unsigned long image_map_size;
    unsigned long rodata_off;
    unsigned long data_off;
    unsigned long bss_off;
    unsigned long bss_end_off;

    if (pid < 0 || pid >= PROC_NUM) {
        return 0;
    }

    l2 = user_pts[pid][0];
    l1_low = user_pts[pid][1];
    l0_image = user_pts[pid][2];
    l0_stack = user_pts[pid][3];
    l1_kernel = user_pts[pid][4];

    for (unsigned long t = 0; t < KERNEL_L0_TABLES; t++) {
        l0_kernel[t] = user_pts[pid][5 + t];
    }

    for (unsigned long table = 0; table < USER_PT_PAGE_COUNT; table++) {
        for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
            user_pts[pid][table][i] = 0;
        }
    }

    for (unsigned long i = 0; i < USER_IMAGE_MAX_SIZE; i++) {
        user_image_pages[pid][i] = 0;
    }

    stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;

    l2[vpn2(USER_BASE)] = pa_to_pte((unsigned long)l1_low) | PTE_V;
    l1_low[vpn1(USER_TEXT_BASE)] = pa_to_pte((unsigned long)l0_image) | PTE_V;
    l1_low[vpn1(stack_bottom)] = pa_to_pte((unsigned long)l0_stack) | PTE_V;

    image_size = (unsigned long)(__userdata_end - __usertext_start);
    image_map_size = page_up((unsigned long)(__userbss_end - __usertext_start));

    rodata_off = (unsigned long)(__userrodata_start - __usertext_start);
    data_off = (unsigned long)(__userdata_start - __usertext_start);
    bss_off = (unsigned long)(__userbss_start - __usertext_start);
    bss_end_off = (unsigned long)(__userbss_end - __usertext_start);

    if (image_map_size > USER_IMAGE_MAX_SIZE) {
        print_str("[KERNEL] user image too large\n");
        return 0;
    }

    for (unsigned long i = 0; i < image_size; i++) {
        user_image_pages[pid][i] = __usertext_start[i];
    }

    for (unsigned long off = 0; off < image_map_size; off += PAGE_SIZE) {
        unsigned long perm;

        if (off < rodata_off) {
            perm = PTE_R | PTE_X | PTE_U;
        } else if (off < data_off) {
            perm = PTE_R | PTE_U;
        } else {
            perm = PTE_R | PTE_W | PTE_U; // bss
        }

        vm_map_page((pagetable_t)l2,
                    USER_TEXT_BASE + off,
                    (unsigned long)user_image_pages[pid] + off,
                    perm);
    }

    vm_map_page((pagetable_t)l2,
                stack_bottom,
                (unsigned long)user_stack_pages[pid],
                PTE_R | PTE_W | PTE_U);

    l2[vpn2(KERNEL_MAP_BASE)] = pa_to_pte((unsigned long)l1_kernel) | PTE_V;

    for (unsigned long t = 0; t < KERNEL_L0_TABLES; t++) {
        unsigned long chunk_base = KERNEL_MAP_BASE + (unsigned long)t * KERNEL_L0_SPAN;

        l1_kernel[vpn1(chunk_base)] = pa_to_pte((unsigned long)l0_kernel[t]) | PTE_V;

        for (unsigned long off = 0; off < KERNEL_L0_SPAN; off += PAGE_SIZE) {
            unsigned long va = chunk_base + off;
            unsigned long pa = chunk_base + off;

            vm_map_page((pagetable_t)l2, va, pa, PTE_R | PTE_W | PTE_X);
        }
    }

    return (pagetable_t)l2;
}
```

### vm.h

```plaintext
// vm.h
#ifndef VM_H
#define VM_H

#include "memlayout.h"

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)

#define SATP_MODE_SV39  (8UL << 60)

typedef unsigned long pte_t;
typedef unsigned long pagetable_t;

void vm_init(void);
void vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long perm);
pagetable_t vm_make_user_pagetable(int pid);
unsigned long vm_make_satp(pagetable_t pt);
void vm_switch_to_user(pagetable_t pt);

#endif
```

