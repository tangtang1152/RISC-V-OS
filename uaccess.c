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
