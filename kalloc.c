#include "kalloc.h"
#include "memlayout.h"
#include "sbi.h"

extern char __kernel_end[];

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list;
static struct free_page *free_tail;
static unsigned long free_page_count;

static unsigned long page_up_local(unsigned long x) {
    return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static int page_aligned(unsigned long x) {
    return (x & (PAGE_SIZE - 1)) == 0;
}

static void page_zero(void *pa) {
    unsigned char *p = (unsigned char *)pa;

    for (unsigned long i = 0; i < PAGE_SIZE; i++) {
        p[i] = 0;
    }
}

/* FIFO: insert at tail so recently freed pages are NOT immediately
 * re-allocated.  This prevents the exec() problem where vm_space_reset
 * frees old page table pages, then vm_space_alloc_page immediately
 * grabs the same physical pages back — corrupting the active SATP. */
static void kfree_page_unchecked(void *pa) {
    struct free_page *page = (struct free_page *)pa;

    page->next = 0;

    if (free_tail) {
        free_tail->next = page;
    } else {
        free_list = page;
    }

    free_tail = page;
    free_page_count++;
}

void kalloc_init(void) {
    // 之前bug：
    // 链接器把 current（BSS 段最后一个变量）和 __kernel_end（Page-aligned 尾部标记）放在了同一个地址 0x80212000。
    // FIFO 分配器从此地址分配的第一个页就是 user0 的 L2 页表，page_zero 和 current = &procs[0] 互相覆盖，破坏了页表。
    // 在linker.Id里改不了 于是直接在kalloc_init里把起始地址往后挪一个页，避开 current 变量所在的页。
    unsigned long start = page_up_local((unsigned long)__kernel_end) + PAGE_SIZE;
    unsigned long end = KALLOC_PHYSTOP;

    free_list = 0;
    free_tail = 0;
    free_page_count = 0;

    for (unsigned long pa = start; pa + PAGE_SIZE <= end; pa += PAGE_SIZE) {
        kfree_page_unchecked((void *)pa);
    }

    print_str("kalloc init: free pages=");
    print_hex(free_page_count);
    print_str("\n");
}

void *kalloc_page(void) {
    struct free_page *page;

    if (!free_list) {
        return 0;
    }

    page = free_list;
    free_list = page->next;

    if (!free_list) {
        free_tail = 0;
    }

    free_page_count--;

    page_zero(page);
    return page;
}

void kfree_page(void *pa) {
    unsigned long p = (unsigned long)pa;
    unsigned long start = page_up_local((unsigned long)__kernel_end) + PAGE_SIZE;

    if (!pa || !page_aligned(p) || p < start || p >= KALLOC_PHYSTOP) {
        print_str("[KERNEL] kfree_page: invalid page\n");
        return;
    }

    kfree_page_unchecked(pa);
}

unsigned long kalloc_free_pages(void) {
    return free_page_count;
}