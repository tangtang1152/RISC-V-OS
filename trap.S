.section .text
.globl kernel_entry
.globl user_entry

.balign 4
kernel_entry:
    addi sp, sp, -80

    sd ra, 0(sp)
    sd sp, 8(sp)        # user-mode sp

    sd a0, 16(sp)
    sd a1, 24(sp)
    sd a2, 32(sp)
    sd a3, 40(sp)
    sd a4, 48(sp)
    sd a5, 56(sp)
    sd a6, 64(sp)
    sd a7, 72(sp)

    mv a0, sp          # a0 = trap_frame*
    call trap_handler

    ld ra, 0(sp)
    ld sp, 8(sp)

    ld a0, 16(sp)
    ld a1, 24(sp)
    ld a2, 32(sp)
    ld a3, 40(sp)
    ld a4, 48(sp)
    ld a5, 56(sp)
    ld a6, 64(sp)
    ld a7, 72(sp)

    addi sp, sp, 80
    sret

.balign 4
user_entry:
    call user_main
1:
    j 1b

.section .rodata
.balign 4
msg:
    .asciz "hello from user mode syscall\n"