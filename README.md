# RISC-V-OS

A tiny RISC-V operating system project built from scratch for learning.

## Current status

This repository currently contains a minimal bootable kernel that can:

- boot on QEMU's RISC-V `virt` machine
- enter the kernel entry point `_start`
- set up a stack
- jump into the first C function `kmain`
- print a string through OpenSBI
- stop in a wait loop

## Project structure

- `start.S` — kernel entry code, sets up the stack and jumps to `kmain`
- `main.c` — the first C code executed by the kernel
- `sbi.c` / `sbi.h` — simple OpenSBI call wrappers and string output
- `linker.ld` — linker script, places the kernel at `0x80200000`
- `Makefile` — build and run commands

## Build

```bash
make