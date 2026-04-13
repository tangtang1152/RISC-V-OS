#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

/*
 * Intended future user virtual address layout.
 */
#define USER_BASE             0x0000000000001000UL
#define USER_TEXT_BASE        USER_BASE
#define USER_RODATA_BASE      (USER_TEXT_BASE + USER_TEXT_MAX_SIZE)
#define USER_TEXT_MAX_SIZE    0x0000000000010000UL   /* 64 KiB */
#define USER_RODATA_MAX_SIZE  0x0000000000010000UL   /* 64 KiB */
#define USER_STACK_TOP        0x0000000040000000UL
#define USER_STACK_SIZE       0x0000000000001000UL

/*
 * Shared kernel mapping window used by the first VM-enabled version.
 * We map a small high-address kernel region into every user page table,
 * supervisor-only by default, then selectively mark a user-code window
 * as user executable.
 */
#define KERNEL_MAP_BASE       0x0000000080200000UL
#define KERNEL_MAP_SIZE       0x0000000001000000UL   /* 16 MiB */
#define KERNEL_L0_SPAN        0x0000000000200000UL   /* 2 MiB per L0 table */
#define KERNEL_L0_TABLES      (KERNEL_MAP_SIZE / KERNEL_L0_SPAN) // 8

#define USER_CODE_WINDOW_SIZE 0x0000000000010000UL   /* 64 KiB */

#define PAGE_SIZE             4096UL
#define PAGE_SHIFT            12UL
#define PT_ENTRY_COUNT        512UL

#endif