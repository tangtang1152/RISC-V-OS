#include "uaccess.h"
#include "riscv.h"

#define SSTATUS_SUM (1UL << 18)

int copyin(const void *uaddr, void *kaddr, unsigned long len) {
    const unsigned char *src = (const unsigned char *)uaddr;
    unsigned char *dst = (unsigned char *)kaddr;
    unsigned long sstatus;

    if ((!uaddr && len != 0) || (!kaddr && len != 0)) {
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