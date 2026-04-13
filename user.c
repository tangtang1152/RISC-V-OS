#define USER_TEXT   __attribute__((section(".usertext")))
#define USER_RODATA __attribute__((section(".userrodata")))
#include "syscall.h"

static const char u0_pid[] USER_RODATA = "[USER0] pid=";
static const char u0_magic[] USER_RODATA = " magic=";
static const char u0_add[] USER_RODATA = " add(20,22)=";
static const char u0_nl[] USER_RODATA = "\n";
static const char u0_wait[] USER_RODATA = "[USER0] wait pid=1\n";
static const char u0_wait_ret[] USER_RODATA = "[USER0] wait returned, exit code=";
static const char u0_pass[] USER_RODATA = "[USER0] TEST PASS\n";
static const char u0_fail[] USER_RODATA = "[USER0] TEST FAIL\n";

static const char u1_pid[] USER_RODATA = "[USER1] pid=";
static const char u1_add[] USER_RODATA = " add(10,32)=";
static const char u1_sleep[] USER_RODATA = "[USER1] sleep 3 ticks\n";
static const char u1_exit[] USER_RODATA = "[USER1] exit now\n";

USER_TEXT void user_main(void)
{
    long pid = sys_getpid();
    long magic = sys_get_magic();
    long sum = sys_add(20, 22);

    sys_printstr(u0_pid);
    sys_printhex((unsigned long)pid);
    sys_printstr(u0_magic);
    sys_printhex((unsigned long)magic);
    sys_printstr(u0_add);
    sys_printhex((unsigned long)sum);
    sys_printstr(u0_nl);

    sys_printstr(u0_wait);
    long code = sys_wait(1);
    sys_printstr(u0_wait_ret);
    sys_printhex((unsigned long)code);
    sys_printstr(u0_nl);

    if (code == 42 && sum == 42 && magic == 'Z') {
        sys_printstr(u0_pass);
    } else {
        sys_printstr(u0_fail);
    }

    sys_exit(0);
    while (1) { }
}

USER_TEXT void user_main2(void)
{
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

USER_TEXT long sys_wait(long pid) {
    return do_syscall1(SYS_WAIT, pid);
}

USER_TEXT long sys_getpid(void) {
    return do_syscall0(SYS_GETPID);
}
