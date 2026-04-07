#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

/*
 * Intended future user virtual address layout.
 * These constants are documentation-oriented for now and will become
 * active once per-process page tables are wired into trap return.
 */

#define USER_BASE        0x0000000000001000UL
#define USER_STACK_TOP   0x0000000040000000UL
#define USER_STACK_SIZE  0x0000000000001000UL

/* RISC-V Sv39 page geometry */
#define PAGE_SIZE        4096UL
#define PAGE_SHIFT       12UL
#define PT_ENTRY_COUNT   512UL

#endif