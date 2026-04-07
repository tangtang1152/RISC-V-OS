#include "vm.h"
#include "proc.h"
#include "sbi.h"

static pte_t kernel_pt_l2[PT_ENTRY_COUNT] __attribute__((aligned(PAGE_SIZE)));
static pte_t user_pts[PROC_NUM][4][PT_ENTRY_COUNT] __attribute__((aligned(PAGE_SIZE)));

/*
 * Current stage:
 * - only builds minimal page-table scaffolding
 * - not yet wired into trap return / satp switching
 * - uses statically reserved page-table pages instead of a real page allocator
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

static inline unsigned long pa_to_pte(unsigned long pa) {
    return ((pa >> PAGE_SHIFT) << 10);
}

static inline unsigned long pte_to_pa(pte_t pte) {
    return ((pte >> 10) << PAGE_SHIFT);
}

void vm_init(void) {
    for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
        kernel_pt_l2[i] = 0;
    }

    for (int p = 0; p < PROC_NUM; p++) {
        for (int level = 0; level < 4; level++) {
            for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
                user_pts[p][level][i] = 0;
            }
        }
    }

    print_str("vm scaffold init done\n");
}

void vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long perm) {
    pte_t *l2 = (pte_t *)pt; // 格式转换
    pte_t *l1;
    pte_t *l0;

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

    l0[vpn0(va)] = pa_to_pte(pa) | perm | PTE_V;
}

pagetable_t vm_make_user_pagetable(int pid) {
    pte_t *l2;
    pte_t *l1;
    pte_t *l0_text;
    pte_t *l0_stack;
    unsigned long stack_bottom;

    if (pid < 0 || pid >= PROC_NUM) {
        return 0;
    }

    l2 = user_pts[pid][0];
    l1 = user_pts[pid][1];
    l0_text = user_pts[pid][2];
    l0_stack = user_pts[pid][3];

    for (unsigned long i = 0; i < PT_ENTRY_COUNT; i++) {
        l2[i] = 0;
        l1[i] = 0;
        l0_text[i] = 0;
        l0_stack[i] = 0;
    }

    stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;

    /*
     * Minimal per-process user page table:
     *   L2(root) -> L1
     *   L1[vpn1(USER_BASE)]   -> L0_text
     *   L1[vpn1(stack_bottom)] -> L0_stack
     *
     * This is enough because:
     * - user text is mapped at USER_BASE
     * - one user stack page is mapped at [stack_bottom, USER_STACK_TOP)
     */
    l2[vpn2(USER_BASE)] = pa_to_pte((unsigned long)l1) | PTE_V;
    l1[vpn1(USER_BASE)] = pa_to_pte((unsigned long)l0_text) | PTE_V;
    l1[vpn1(stack_bottom)] = pa_to_pte((unsigned long)l0_stack) | PTE_V;

    vm_map_page((pagetable_t)l2,
                USER_BASE,
                user_entry_pa_for_pid(pid),
                PTE_R | PTE_X | PTE_U);

    vm_map_page((pagetable_t)l2,
                stack_bottom,
                (unsigned long)procs[pid].ustack,
                PTE_R | PTE_W | PTE_U);

    return (pagetable_t)l2;
}

unsigned long vm_make_satp(pagetable_t pt) {
    return SATP_MODE_SV39 | (pt >> PAGE_SHIFT);
}

static unsigned long user_entry_pa_for_pid(int pid) {
    if (pid == 0) {
        return (unsigned long)user_entry;
    }
    return (unsigned long)user_entry2;
}