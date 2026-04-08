#ifndef VM_H
#define VM_H

#include "memlayout.h"

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)

#define SATP_MODE_SV39  (8UL << 60)

typedef unsigned long pte_t;
typedef unsigned long pagetable_t;

void vm_init(void);
void vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long perm);
pagetable_t vm_make_user_pagetable(int pid);
unsigned long vm_make_satp(pagetable_t pt);
void vm_switch_to_user(pagetable_t pt);

#endif