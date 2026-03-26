.section .text
.globl kernel_entry
.globl user_entry

.balign 4
kernel_entry:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)

    csrr s0, scause
    csrr s1, sepc
    csrr s2, stval
    mv   s3, a0       # save user a0
    mv   s4, a7       # save user a7

    mv a0, s0         # scause
    mv a1, s1         # sepc
    mv a2, s2         # stval
    mv a3, s3         # user a0
    mv a4, s4         # user a7
    call trap_handler

    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    sret

.balign 4
user_entry:
    li a7, 2
    la a0, msg
    ecall
1:
    j 1b

.section .rodata
.balign 4
msg:
    .asciz "hello from user mode syscall\n"