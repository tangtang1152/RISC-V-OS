#include "syscall.h"

void user_main(void)
{
    sys_printstr("[USER0] wait pid=1\n");
    sys_wait(1);
    sys_printstr("[USER0] wait returned, pid=1 exited\n");

    while (1) {
        sys_yield();
    }
}
void user_main2(void)
{
    sys_printstr("[USER1] exit now\n");
    sys_exit(0);

    while (1) { }
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
long sys_wait(long pid) {
    return do_syscall1(SYS_WAIT, pid);
}
long sys_getpid(void) {
    return do_syscall0(SYS_GETPID);
}