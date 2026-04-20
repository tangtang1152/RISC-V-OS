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

int vm_handle_user_page_fault(int pid, pagetable_t pt, unsigned long scause, unsigned long fault_va) {
    user_layout_t layout;
    unsigned long va_page;
    unsigned long page_off;
    unsigned long pa;
    pte_t *pte;

    if (pid < 0 || pid >= PROC_NUM || !pt) {
        return -1;
    }

    if (scause != 13 && scause != 15) {
        return -1;
    }

    vm_user_layout_init(&layout);

    // fault_va 就是报错的stval
    if (!vm_user_is_demand_range(&layout, fault_va)) {
        return -1;
    }

    va_page = page_down(fault_va);
    page_off = va_page - USER_TEXT_BASE;

    if (page_off >= USER_IMAGE_MAX_SIZE) {
        return -1;
    }

    // 已经有映射或者权限问题 就不做
    pte = vm_walk(pt, va_page);
    if (pte && (*pte & PTE_V)) {
        return -1;
    }

    /*
     * Phase-1 backing:
     * still use the pre-reserved user_image_pages[pid] as physical backing.
     * vm_init/vm_make_user_pagetable already zero this area,
     * so BSS pages naturally come in as zero-filled.
     */
    pa = (unsigned long)user_image_pages[pid] + page_off;

    vm_map_page(pt, va_page, pa, PTE_R | PTE_W | PTE_U);
    // 刷新 TLB
    sfence_vma();

    print_str("[KERNEL] demand map: pid=");
    print_hex((unsigned long)pid);
    print_str(" va=");
    print_hex(va_page);
    print_str("\n");

    return 0;
}

pagetable_t vm_make_user_pagetable(int pid) {
    pte_t *l2;
    pte_t *l1_low;
    pte_t *l0_image;
    pte_t *l0_stack;
    pte_t *l1_kernel;
    pte_t *l0_kernel[KERNEL_L0_TABLES];
    unsigned long stack_bottom;
    unsigned long image_size;
    unsigned long image_map_size;
    unsigned long rodata_off;
    unsigned long data_off;
    unsigned long bss_off;
    unsigned long bss_end_off;

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

    user_layout_t layout;
    vm_user_layout_init(&layout);

    l2[vpn2(USER_BASE)] = pa_to_pte((unsigned long)l1_low) | PTE_V;
    l1_low[vpn1(USER_TEXT_BASE)] = pa_to_pte((unsigned long)l0_image) | PTE_V;
    l1_low[vpn1(layout.stack_bottom)] = pa_to_pte((unsigned long)l0_stack) | PTE_V;

    if (layout.full_image_size > USER_IMAGE_MAX_SIZE) {
        print_str("[KERNEL] user image too large\n");
        return 0;
    }

    for (unsigned long i = 0; i < layout.image_copy_size; i++) {
        user_image_pages[pid][i] = __usertext_start[i];
    }

    /*
     * Phase-1 split:
     *   eager map  = text / rodata / data
     *   lazy map   = bss .. demand_end
     *
     * So here we only pre-map the eager portion.
     */
    for (unsigned long off = 0; off < layout.eager_map_size; off += PAGE_SIZE) {
        unsigned long va = USER_TEXT_BASE + off;
        unsigned long perm = vm_user_image_perm(&layout, va);

        if (perm == 0) {
            print_str("[KERNEL] unexpected eager user image va perm lookup miss\n");
            return 0;
        }

        vm_map_page((pagetable_t)l2,
                    va,
                    (unsigned long)user_image_pages[pid] + off,
                    perm);
    }

    vm_map_page((pagetable_t)l2,
                layout.stack_bottom,
                (unsigned long)user_stack_pages[pid],
                PTE_R | PTE_W | PTE_U);

    l2[vpn2(KERNEL_MAP_BASE)] = pa_to_pte((unsigned long)l1_kernel) | PTE_V;

    for (unsigned long t = 0; t < KERNEL_L0_TABLES; t++) {
        unsigned long chunk_base = KERNEL_MAP_BASE + (unsigned long)t * KERNEL_L0_SPAN;

        l1_kernel[vpn1(chunk_base)] = pa_to_pte((unsigned long)l0_kernel[t]) | PTE_V;

        for (unsigned long off = 0; off < KERNEL_L0_SPAN; off += PAGE_SIZE) {
            unsigned long va = chunk_base + off;
            unsigned long pa = chunk_base + off;

            vm_map_page((pagetable_t)l2, va, pa, PTE_R | PTE_W | PTE_X);
        }
    }

    return (pagetable_t)l2;
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

