#ifndef KALLOC_H
#define KALLOC_H

void kalloc_init(void);
void *kalloc_page(void);
void kfree_page(void *pa);
unsigned long kalloc_free_pages(void);

#endif
