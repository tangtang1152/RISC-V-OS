#include "uaccess.h"
#include "memlayout.h"
#include "proc.h"
#include "vm.h"

static int ensure_user_access(unsigned long va, vm_access_t access, unsigned long *pa_out) {
    if (!current) {
        return -1;
    }

    return vm_ensure_user_access(current->pid,
                                 current->user_pagetable,
                                 va,
                                 access,
                                 pa_out);
}

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
    unsigned long src_va = (unsigned long)uaddr;
    unsigned char *dst = (unsigned char *)kaddr;

    if ((!uaddr && len != 0) || (!kaddr && len != 0)) {
        return -1;
    }
    if (!user_range_ok(uaddr, len)) {
        return -1;
    }

    while (len > 0) {
        // len比此页剩下空间大 多的就得分到下一页
        unsigned long page_left = PAGE_SIZE - (src_va & (PAGE_SIZE - 1));
        unsigned long chunk = (len < page_left) ? len : page_left;
        unsigned long src_pa;

        if (ensure_user_access(src_va, VM_ACCESS_READ, &src_pa) < 0) {
            return -1;
        }

        for (unsigned long i = 0; i < chunk; i++) {
            dst[i] = ((unsigned char *)src_pa)[i];
        }

        src_va += chunk;
        dst += chunk;
        len -= chunk;
    }

    return 0;
}

int copyout(void *uaddr, const void *kaddr, unsigned long len) {
    unsigned long dst_va = (unsigned long)uaddr;
    const unsigned char *src = (const unsigned char *)kaddr;

    if ((!uaddr && len != 0) || (!kaddr && len != 0)) {
        return -1;
    }
    if (!user_range_ok(uaddr, len)) {
        return -1;
    }

    while (len > 0) {
        unsigned long page_left = PAGE_SIZE - (dst_va & (PAGE_SIZE - 1));
        unsigned long chunk = (len < page_left) ? len : page_left;
        unsigned long dst_pa;

        if (ensure_user_access(dst_va, VM_ACCESS_WRITE, &dst_pa) < 0) {
            return -1;
        }

        for (unsigned long i = 0; i < chunk; i++) {
            ((unsigned char *)dst_pa)[i] = src[i];
        }

        dst_va += chunk;
        src += chunk;
        len -= chunk;
    }

    return 0;
}

int copyinstr(const char *uaddr, char *kbuf, unsigned long maxlen) {
    unsigned long src_va = (unsigned long)uaddr;

    if (!uaddr || !kbuf || maxlen == 0) {
        return -1;
    }

    for (unsigned long i = 0; i < maxlen; i++) {
        unsigned long src_pa;
        char ch;

        if (!user_range_ok((const void *)src_va, 1)) {
            return -1;
        }

        if (ensure_user_access(src_va, VM_ACCESS_READ, &src_pa) < 0) {
            return -1;
        }

        ch = *(char *)src_pa;
        kbuf[i] = ch;

        if (ch == '\0') {
            return 0;
        }

        src_va++;
    }

    kbuf[maxlen - 1] = '\0';
    return -1;
}
