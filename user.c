#include "syscall.h"

void user_main(void)
{
    while (1) {
        sys_printstr("[USER1] before sleep\n");
        sys_sleep(5);
        sys_printstr("[USER1] after sleep\n");
        for (volatile int i = 0; i < 100000; i++) { }
    }
}

void user_main2(void)
{
    while (1) {
        sys_printstr("[USER2] running\n");
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
long sys_sleep(long tick_count) {
    return do_syscall1(SYS_SLEEP, tick_count);
}