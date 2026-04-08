#include "vm.h"
#include "proc.h"
#include "sbi.h"
#include "riscv.h"
#include "memlayout.h"

extern char __usertext_start[];
extern char __usertext_end[];

#define USER_PT_PAGE_COUNT (4 + KERNEL_L0_TABLES)

static pte_t user_pts[PROC_NUM][USER_PT_PAGE_COUNT][PT_ENTRY_COUNT]
    __attribute__((aligned(PAGE_SIZE)));

static unsigned char user_stack_pages[PROC_NUM][USER_STACK_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

/*
 * Layout per process:
 *   [0] root L2
 *   [1] low L1 (for USER_STACK_TOP region)
 *   [2] low L0 (stack leaf table)
 *   [3] kernel L1
 *   [4..] kernel L0 tables covering KERNEL_MAP_SIZE
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

void vm_init(void) {
    for (int p = 0; p < PROC_NUM; p++) {
        for (int table = 0; table < USER_PT_PAGE_COUNT; table++) {
            for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
                user_pts[p][table][i] = 0;
            }
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

pagetable_t vm_make_user_pagetable(int pid) {
    pte_t *l2;
    pte_t *l1_low;
    pte_t *l0_stack;
    pte_t *l1_kernel;
    pte_t *l0_kernel[KERNEL_L0_TABLES];
    unsigned long stack_bottom;
    unsigned long code_start;
    unsigned long code_end;

    if (pid < 0 || pid >= PROC_NUM) {
        return 0;
    }

    l2 = user_pts[pid][0];
    l1_low = user_pts[pid][1];
    l0_stack = user_pts[pid][2];
    l1_kernel = user_pts[pid][3];

    for (int t = 0; t < KERNEL_L0_TABLES; t++) {
        l0_kernel[t] = user_pts[pid][4 + t];
    }

    for (int table = 0; table < USER_PT_PAGE_COUNT; table++) {
        for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
            user_pts[pid][table][i] = 0;
        }
    }

    stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;

    /*
     * Low user region for stack.
     */
    l2[vpn2(stack_bottom)] = pa_to_pte((unsigned long)l1_low) | PTE_V;
    l1_low[vpn1(stack_bottom)] = pa_to_pte((unsigned long)l0_stack) | PTE_V;

    vm_map_page((pagetable_t)l2,
                stack_bottom,
                (unsigned long)user_stack_pages[pid],
                PTE_R | PTE_W | PTE_U);

    /*
     * Shared kernel mapping window.
     * Supervisor-only by default.
     */
    l2[vpn2(KERNEL_MAP_BASE)] = pa_to_pte((unsigned long)l1_kernel) | PTE_V;

    for (int t = 0; t < KERNEL_L0_TABLES; t++) {
        unsigned long chunk_base = KERNEL_MAP_BASE + (unsigned long)t * KERNEL_L0_SPAN;

        l1_kernel[vpn1(chunk_base)] = pa_to_pte((unsigned long)l0_kernel[t]) | PTE_V;

        for (unsigned long off = 0; off < KERNEL_L0_SPAN; off += PAGE_SIZE) {
            unsigned long va = chunk_base + off;
            unsigned long pa = chunk_base + off;

            vm_map_page((pagetable_t)l2, va, pa, PTE_R | PTE_W | PTE_X);
        }
    }

    code_start = page_down((unsigned long)__usertext_start);
    code_end = page_up((unsigned long)__usertext_end);

    for (unsigned long va = code_start; va < code_end; va += PAGE_SIZE) {
        vm_map_page((pagetable_t)l2, va, va, PTE_R | PTE_X | PTE_U);
    }

    return (pagetable_t)l2;
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