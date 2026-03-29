#include "syscall.h"

void user_main(void) {
    sys_printstr("hello from user c\n");
    long x = sys_add(40, 2);
    sys_putchar((char)x);
    sys_exit(0);
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