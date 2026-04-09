#define USER_TEXT __attribute__((section(".usertext")))
#include "syscall.h"

USER_TEXT void user_main(void)
{
    long pid = sys_getpid();
    long magic = sys_get_magic();
    long sum = sys_add(20, 22);

    sys_printstr("[USER0] pid=");
    sys_printhex((unsigned long)pid);
    sys_printstr(" magic=");
    sys_printhex((unsigned long)magic);
    sys_printstr(" add(20,22)=");
    sys_printhex((unsigned long)sum);
    sys_printstr("\n");

    sys_printstr("[USER0] wait pid=1\n");
    long code = sys_wait(1);
    sys_printstr("[USER0] wait returned, exit code=");
    sys_printhex((unsigned long)code);
    sys_printstr("\n");

    if (code == 42 && sum == 42 && magic == 'Z') {
        sys_printstr("[USER0] TEST PASS\n");
    } else {
        sys_printstr("[USER0] TEST FAIL\n");
    }

    sys_exit(0);
    while (1) { }
}

USER_TEXT void user_main2(void)
{
    long pid = sys_getpid();
    long sum = sys_add(10, 32);

    sys_printstr("[USER1] pid=");
    sys_printhex((unsigned long)pid);
    sys_printstr(" add(10,32)=");
    sys_printhex((unsigned long)sum);
    sys_printstr("\n");

    sys_printstr("[USER1] sleep 3 ticks\n");
    sys_sleep(3);
    sys_yield();

    sys_printstr("[USER1] exit now\n");
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

USER_TEXT long sys_wait(long pid) {
    return do_syscall1(SYS_WAIT, pid);
}

USER_TEXT long sys_getpid(void) {
    return do_syscall0(SYS_GETPID);
}
