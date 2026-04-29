#include "kalloc.h"
#include "memlayout.h"
#include "sbi.h"

extern char __kernel_end[];

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list;
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

static void kfree_page_unchecked(void *pa) {
    struct free_page *page = (struct free_page *)pa;

    page->next = free_list;
    free_list = page;
    free_page_count++;
}

void kalloc_init(void) {
    unsigned long start = page_up_local((unsigned long)__kernel_end);
    unsigned long end = KALLOC_PHYSTOP;

    free_list = 0;
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
    free_page_count--;

    page_zero(page);
    return page;
}

void kfree_page(void *pa) {
    unsigned long p = (unsigned long)pa;
    unsigned long start = page_up_local((unsigned long)__kernel_end);

    if (!pa || !page_aligned(p) || p < start || p >= KALLOC_PHYSTOP) {
        print_str("[KERNEL] kfree_page: invalid page\n");
        return;
    }

    kfree_page_unchecked(pa);
}

unsigned long kalloc_free_pages(void) {
    return free_page_count;
}
