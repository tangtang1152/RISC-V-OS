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

typedef struct {
    unsigned long image_copy_size;    /* text + rodata + data */
    unsigned long eager_map_size;     /* eager mapped: text + rodata + data */
    unsigned long full_image_size;    /* full image span: text + rodata + data + bss */

    unsigned long text_start;
    unsigned long text_end;

    unsigned long rodata_start;
    unsigned long rodata_end;

    unsigned long data_start;
    unsigned long data_end;

    unsigned long bss_start;
    unsigned long bss_end;

    unsigned long demand_start;       /* phase-1 lazy region start */
    unsigned long demand_end;         /* phase-1 lazy region end */

    unsigned long stack_bottom;
    unsigned long stack_top;
} user_layout_t;

typedef enum {
    USER_IMAGE_STATIC_LINKED = 0,
} user_image_kind_t;
typedef struct {
    const char *name;
    user_image_kind_t kind;

    /*
     * Phase-1 static image source:
     * file-backed part only (text + rodata + data),
     * excluding bss which stays zero-fill / lazy-mapped.
     */
    const unsigned char *source_base;
    unsigned long source_size;

    unsigned long entry_offset;
    user_layout_t layout;
} user_image_desc;

typedef enum {
    VM_ACCESS_READ = 1,
    VM_ACCESS_WRITE = 2,
    VM_ACCESS_EXEC = 4,
} vm_access_t;

void vm_init(void);
void vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long perm);
pagetable_t vm_make_user_pagetable(int pid, const user_image_desc *image);

unsigned long vm_make_satp(pagetable_t pt);
void vm_switch_to_user(pagetable_t pt);

void vm_user_layout_init(user_layout_t *layout);
void vm_build_static_user_image_desc(user_image_desc *image,
                                     const char *name,
                                     unsigned long entry_offset);
int vm_user_range_contains(unsigned long va, unsigned long start, unsigned long end);
unsigned long vm_user_image_perm(const user_layout_t *layout, unsigned long va);
int vm_user_is_demand_range(const user_layout_t *layout, unsigned long va);

pte_t *vm_walk(pagetable_t pt, unsigned long va);

/*
 * Pure query:
 *   return 0 on success and store translated PA in *pa_out
 *   return -1 if unmapped / permission denied / invalid
 */
int vm_translate_user(pagetable_t pt,
                      unsigned long va,
                      vm_access_t access,
                      unsigned long *pa_out);

/*
 * Stateful ensure:
 *   if already accessible -> success
 *   if lazily mappable   -> map and then success
 *   else                 -> fail
 */
int vm_ensure_user_access(int pid,
                          pagetable_t pt,
                          unsigned long va,
                          vm_access_t access,
                          unsigned long *pa_out);
#endif