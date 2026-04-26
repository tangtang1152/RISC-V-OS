#include "vm.h"
#include "proc.h"
#include "sbi.h"
#include "riscv.h"
#include "memlayout.h"

extern char __userimage_start[];
extern char __userimage_end[];
extern char __usertext_start[];
extern char __usertext_end[];
extern char __userrodata_start[];
extern char __userrodata_end[];
extern char __userdata_start[];
extern char __userdata_end[];
extern char __userbss_start[];
extern char __userbss_end[];

#define USER_PT_PAGE_COUNT (5 + KERNEL_L0_TABLES)

static pte_t user_pts[PROC_NUM][USER_PT_PAGE_COUNT][PT_ENTRY_COUNT]
    __attribute__((aligned(PAGE_SIZE)));
static unsigned char user_image_pages[PROC_NUM][USER_IMAGE_MAX_SIZE]
    __attribute__((aligned(PAGE_SIZE)));
static unsigned char user_stack_pages[PROC_NUM][USER_STACK_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

/*
 * Layout per process:
 *   [0] root L2
 *   [1] low L1 (for low user region)
 *   [2] low L0 (user image leaf table)
 *   [3] low L0 (stack leaf table)
 *   [4] kernel L1
 *   [5..] kernel L0 tables covering KERNEL_MAP_SIZE
 */

static inline unsigned long vpn2(unsigned long va) {
    return (va >> 30) & 0x1ffUL;
}
static inline unsigned long vpn1(unsigned long va) {
    return (va >> 21) & 0x1ffUL;
}
static inline unsigned long vpn0(unsigned long va) {
    return (va >> 12) & 0x1ffUL;
}

static inline unsigned long page_down(unsigned long x) {
    return x & ~(PAGE_SIZE - 1);
}
static inline unsigned long page_up(unsigned long x) {
    return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}
static inline unsigned long pa_to_pte(unsigned long pa) {
    return ((pa >> PAGE_SHIFT) << 10);
}
static inline unsigned long pte_to_pa(pte_t pte) {
    return ((pte >> 10) << PAGE_SHIFT);
}

static unsigned long vm_access_to_pte_perm(vm_access_t access) {
    unsigned long perm = 0;

    if (access & VM_ACCESS_READ) {
        perm |= PTE_R;
    }
    if (access & VM_ACCESS_WRITE) {
        perm |= PTE_W;
    }
    if (access & VM_ACCESS_EXEC) {
        perm |= PTE_X;
    }

    return perm;
}

/*
 * Phase-1 lazy mapping policy
 *
 * Supported now:
 *   - lazy range: [layout.demand_start, layout.demand_end)
 *   - current concrete use: USER_BSS + future heap reserve
 *   - backing source: pre-reserved user_image_pages[pid]
 *   - mapped permission: RWU
 *
 * Not supported yet:
 *   - instruction-demand paging for text
 *   - stack growth
 *   - true physical page allocation
 *   - per-segment lazy permission refinement
 */
static int vm_try_map_user_demand_page(int pid,
                                       pagetable_t pt,
                                       unsigned long va,
                                       vm_access_t access) {
    user_layout_t layout;
    unsigned long va_page;
    unsigned long page_off;
    unsigned long pa;
    pte_t *pte;

    (void)access;

    if (pid < 0 || pid >= PROC_NUM || !pt) {
        return -1;
    }

    vm_user_layout_init(&layout);

    if (!vm_user_is_demand_range(&layout, va)) {
        return -1;
    }

    va_page = page_down(va);
    page_off = va_page - USER_TEXT_BASE;

    if (page_off >= USER_IMAGE_MAX_SIZE) {
        return -1;
    }

    pte = vm_walk(pt, va_page);
    if (pte && (*pte & PTE_V)) {
        return 0;
    }

    pa = (unsigned long)user_image_pages[pid] + page_off;

    vm_map_page(pt, va_page, pa, PTE_R | PTE_W | PTE_U);
    sfence_vma();

    print_str("[KERNEL] demand map: pid=");
    print_hex((unsigned long)pid);
    print_str(" va=");
    print_hex(va_page);
    print_str("\n");

    return 0;
}

unsigned long vm_make_satp(pagetable_t pt) {
    return SATP_MODE_SV39 | (pt >> PAGE_SHIFT);
}
void vm_switch_to_user(pagetable_t pt) {
    if (!pt) {
        return;
    }

    w_satp(vm_make_satp(pt));
    sfence_vma();
}

void vm_init(void) {
    for (unsigned long p = 0; p < PROC_NUM; p++) {
        for (unsigned long table = 0; table < USER_PT_PAGE_COUNT; table++) {
            for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
                user_pts[p][table][i] = 0;
            }
        }

        for (unsigned long i = 0; i < USER_IMAGE_MAX_SIZE; i++) {
            user_image_pages[p][i] = 0;
        }

        for (unsigned long i = 0; i < USER_STACK_SIZE; i++) {
            user_stack_pages[p][i] = 0;
        }
    }

    print_str("vm scaffold init done\n");
}

void vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long perm) {
    pte_t *l2 = (pte_t *)pt;
    pte_t *l1;
    pte_t *l0;
    unsigned long leaf_perm = perm | PTE_A;

    if (perm & PTE_W) {
        leaf_perm |= PTE_D;
    }

    if (!(l2[vpn2(va)] & PTE_V)) {
        print_str("[KERNEL] vm_map_page: missing l1 table\n");
        return;
    }
    l1 = (pte_t *)pte_to_pa(l2[vpn2(va)]);

    if (!(l1[vpn1(va)] & PTE_V)) {
        print_str("[KERNEL] vm_map_page: missing l0 table\n");
        return;
    }
    l0 = (pte_t *)pte_to_pa(l1[vpn1(va)]);

    l0[vpn0(va)] = pa_to_pte(pa) | leaf_perm | PTE_V;
}

pte_t *vm_walk(pagetable_t pt, unsigned long va) {
    pte_t *l2 = (pte_t *)pt;
    pte_t *l1;
    pte_t *l0;

    if (!pt) {
        return 0;
    }

    if (!(l2[vpn2(va)] & PTE_V)) {
        return 0;
    }
    l1 = (pte_t *)pte_to_pa(l2[vpn2(va)]);

    if (!(l1[vpn1(va)] & PTE_V)) {
        return 0;
    }
    l0 = (pte_t *)pte_to_pa(l1[vpn1(va)]);

    return &l0[vpn0(va)]; // 返回的不是 PA，而是 PTE 指针
}

static int vm_copy_image_source(int pid, const user_image_desc *image) {
    if (pid < 0 || pid >= PROC_NUM || !image || !image->source_base) {
        return -1;
    }

    if (image->source_size > USER_IMAGE_MAX_SIZE) {
        print_str("[KERNEL] user image too large\n");
        return -1;
    }

    for (unsigned long i = 0; i < image->source_size; i++) {
        user_image_pages[pid][i] = image->source_base[i];
    }

    return 0;
}
/*
 * Phase-1 eager segment mapper:
 * - maps only explicit file-backed eager segments
 * - current static image segments are: text, rodata, data
 * - bss is intentionally excluded and remains lazy-mapped
 */
static int vm_map_image_eager(pagetable_t pt, int pid, const user_image_desc *image) {
    if (!pt || pid < 0 || pid >= PROC_NUM || !image) {
        return -1;
    }

    for (unsigned long s = 0; s < image->segment_count; s++) {
        const user_segment_desc *seg = &image->segments[s];
        unsigned long seg_page_start = page_down(seg->va_start);
        unsigned long seg_page_end = page_up(seg->va_start + seg->size);

        for (unsigned long va = seg_page_start; va < seg_page_end; va += PAGE_SIZE) {
            unsigned long off = va - USER_TEXT_BASE;

            vm_map_page(pt,
                        va,
                        (unsigned long)user_image_pages[pid] + off,
                        seg->perm);
        }
    }

    return 0;
}
static int vm_map_user_stack(pagetable_t pt, int pid, const user_layout_t *layout) {
    if (!pt || pid < 0 || pid >= PROC_NUM || !layout) {
        return -1;
    }

    vm_map_page(pt,
                layout->stack_bottom,
                (unsigned long)user_stack_pages[pid],
                PTE_R | PTE_W | PTE_U);

    return 0;
}
static int vm_map_kernel_window(pagetable_t pt,
                                pte_t *l2,
                                pte_t *l1_kernel,
                                pte_t *l0_kernel[KERNEL_L0_TABLES]) {
    if (!pt || !l2 || !l1_kernel) {
        return -1;
    }

    l2[vpn2(KERNEL_MAP_BASE)] = pa_to_pte((unsigned long)l1_kernel) | PTE_V;

    for (unsigned long t = 0; t < KERNEL_L0_TABLES; t++) {
        unsigned long chunk_base = KERNEL_MAP_BASE + (unsigned long)t * KERNEL_L0_SPAN;

        l1_kernel[vpn1(chunk_base)] = pa_to_pte((unsigned long)l0_kernel[t]) | PTE_V;

        for (unsigned long off = 0; off < KERNEL_L0_SPAN; off += PAGE_SIZE) {
            unsigned long va = chunk_base + off;
            unsigned long pa = chunk_base + off;

            vm_map_page(pt, va, pa, PTE_R | PTE_W | PTE_X);
        }
    }

    return 0;
}
/*
 * Phase-1 static image loader control flow:
 *   1) validate image descriptor and source
 *   2) copy file-backed image source into per-proc backing memory
 *   3) eagerly map text / rodata / data
 *   4) map user stack
 *   5) map shared kernel window
 *
 * Lazy bss / heap reserve is not mapped here.
 * It is resolved later via vm_ensure_user_access().
 */
pagetable_t vm_make_user_pagetable(int pid, const user_image_desc *image) {
    pte_t *l2;
    pte_t *l1_low;
    pte_t *l0_image;
    pte_t *l0_stack;
    pte_t *l1_kernel;
    pte_t *l0_kernel[KERNEL_L0_TABLES];

    if (pid < 0 || pid >= PROC_NUM) {
        return 0;
    }

    l2 = user_pts[pid][0];
    l1_low = user_pts[pid][1];
    l0_image = user_pts[pid][2];
    l0_stack = user_pts[pid][3];
    l1_kernel = user_pts[pid][4];

    for (unsigned long t = 0; t < KERNEL_L0_TABLES; t++) {
        l0_kernel[t] = user_pts[pid][5 + t];
    }

    for (unsigned long table = 0; table < USER_PT_PAGE_COUNT; table++) {
        for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
            user_pts[pid][table][i] = 0;
        }
    }

    for (unsigned long i = 0; i < USER_IMAGE_MAX_SIZE; i++) {
        user_image_pages[pid][i] = 0;
    }

    const user_layout_t *layout;
    if (!image) {
        return 0;
    }
    layout = &image->layout;

    l2[vpn2(USER_BASE)] = pa_to_pte((unsigned long)l1_low) | PTE_V;
    l1_low[vpn1(USER_TEXT_BASE)] = pa_to_pte((unsigned long)l0_image) | PTE_V;
    l1_low[vpn1(layout->stack_bottom)] = pa_to_pte((unsigned long)l0_stack) | PTE_V;

    if (!image->source_base) {
        print_str("[KERNEL] user image source is null\n");
        return 0;
    }
    if (image->source_size != layout->image_copy_size) {
        print_str("[KERNEL] user image source size mismatch\n");
        return 0;
    }

    if (vm_copy_image_source(pid, image) < 0) {
        print_str("[KERNEL] failed to copy user image source\n");
        return 0;
    }

    if (vm_map_image_eager((pagetable_t)l2, pid, image) < 0) {
        print_str("[KERNEL] failed to map eager user image segments\n");
        return 0;
    }

    if (vm_map_user_stack((pagetable_t)l2, pid, layout) < 0) {
        print_str("[KERNEL] failed to map user stack\n");
        return 0;
    }

    if (vm_map_kernel_window((pagetable_t)l2, l2, l1_kernel, l0_kernel) < 0) {
        print_str("[KERNEL] failed to map kernel window\n");
        return 0;
    }

    return (pagetable_t)l2;
}

int vm_translate_user(pagetable_t pt,
                      unsigned long va,
                      vm_access_t access,
                      unsigned long *pa_out) {
    pte_t *pte;
    unsigned long need_perm;

    if (!pt || !pa_out) {
        return -1;
    }

    pte = vm_walk(pt, va);
    if (!pte) {
        return -1;
    }

    if (!(*pte & PTE_V) || !(*pte & PTE_U)) {
        return -1;
    }

    need_perm = vm_access_to_pte_perm(access);

    if ((need_perm & PTE_R) && !(*pte & PTE_R)) {
        return -1;
    }
    if ((need_perm & PTE_W) && !(*pte & PTE_W)) {
        return -1;
    }
    if ((need_perm & PTE_X) && !(*pte & PTE_X)) {
        return -1;
    }

    *pa_out = pte_to_pa(*pte) + (va & (PAGE_SIZE - 1)); // + offset
    return 0;
}

int vm_ensure_user_access(int pid,
                          pagetable_t pt,
                          unsigned long va,
                          vm_access_t access,
                          unsigned long *pa_out) {
    unsigned long pa;

    if (vm_translate_user(pt, va, access, &pa) == 0) {
        if (pa_out) {
            *pa_out = pa;
        }
        return 0;
    }

    if (vm_try_map_user_demand_page(pid, pt, va, access) < 0) {
        return -1;
    }

    // 再试一次
    if (vm_translate_user(pt, va, access, &pa) < 0) {
        return -1;
    }

    if (pa_out) {
        *pa_out = pa;
    }
    return 0;
}

void vm_user_layout_init(user_layout_t *l) {
    if (!l) {
        return;
    }

    l->text_start   = USER_TEXT_BASE;
    l->text_end     = USER_TEXT_BASE + (unsigned long)(__usertext_end - __usertext_start);

    l->rodata_start = USER_TEXT_BASE + (unsigned long)(__userrodata_start - __usertext_start);
    l->rodata_end   = USER_TEXT_BASE + (unsigned long)(__userrodata_end - __usertext_start);

    l->data_start   = USER_TEXT_BASE + (unsigned long)(__userdata_start - __usertext_start);
    l->data_end     = USER_TEXT_BASE + (unsigned long)(__userdata_end - __usertext_start);

    l->bss_start    = USER_TEXT_BASE + (unsigned long)(__userbss_start - __usertext_start);
    l->bss_end      = USER_TEXT_BASE + (unsigned long)(__userbss_end - __usertext_start);

    l->image_copy_size = (unsigned long)(__userdata_end - __usertext_start);
    l->eager_map_size  = page_up((unsigned long)(__userdata_end - __usertext_start));
    l->full_image_size = page_up((unsigned long)(__userbss_end - __usertext_start));

    /*
    * Phase-1 demand paging policy:
    *   eager  = text / rodata / data
    *   lazy   = bss .. USER_TEXT_BASE + USER_IMAGE_MAX_SIZE
    */
    l->demand_start = l->bss_start;
    l->demand_end   = USER_TEXT_BASE + USER_IMAGE_MAX_SIZE;

    l->stack_top    = USER_STACK_TOP;
    l->stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
}
static void vm_build_static_image_segments(user_image_desc *image) {
    user_layout_t *l;

    if (!image) {
        return;
    }

    l = &image->layout;

    image->segment_count = 3;

    image->segments[0].name = "text";
    image->segments[0].va_start = l->text_start;
    image->segments[0].size = l->rodata_start - l->text_start;
    image->segments[0].perm = PTE_R | PTE_X | PTE_U;
    image->segments[0].src_offset = 0;

    image->segments[1].name = "rodata";
    image->segments[1].va_start = l->rodata_start;
    image->segments[1].size = l->data_start - l->rodata_start;
    image->segments[1].perm = PTE_R | PTE_U;
    image->segments[1].src_offset = l->rodata_start - USER_TEXT_BASE;

    image->segments[2].name = "data";
    image->segments[2].va_start = l->data_start;
    image->segments[2].size = l->data_end - l->data_start;
    image->segments[2].perm = PTE_R | PTE_W | PTE_U;
    image->segments[2].src_offset = l->data_start - USER_TEXT_BASE;
}
void vm_build_static_user_image_desc(user_image_desc *image,
                                     const char *name,
                                     unsigned long entry_offset) {
    if (!image) {
        return;
    }

    image->name = name;
    image->kind = USER_IMAGE_STATIC_LINKED;
    image->entry_offset = entry_offset;

    vm_user_layout_init(&image->layout);

    image->source_base = (const unsigned char *)__usertext_start;
    image->source_size = image->layout.image_copy_size;

    vm_build_static_image_segments(image);
}

int vm_user_range_contains(unsigned long va, unsigned long start, unsigned long end) {
    return va >= start && va < end;
}
int vm_user_is_demand_range(const user_layout_t *layout, unsigned long va) {
    return vm_user_range_contains(va, layout->demand_start, layout->demand_end);
}

unsigned long vm_user_image_perm(const user_layout_t *layout, unsigned long va) {
    if (vm_user_range_contains(va, layout->text_start, layout->rodata_start)) {
        return PTE_R | PTE_X | PTE_U;
    }
    if (vm_user_range_contains(va, layout->rodata_start, layout->data_start)) {
        return PTE_R | PTE_U;
    }
    if (vm_user_range_contains(va, layout->data_start, layout->bss_end)) {
        return PTE_R | PTE_W | PTE_U;
    }

    return 0;
}

