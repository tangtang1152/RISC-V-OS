.section .text
.globl kernel_entry
.globl user_entry

.balign 4
kernel_entry:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)

    csrr s0, scause
    csrr s1, sepc
    csrr s2, stval
    mv   s3, a0      # user a0
    mv   s4, a1      # user a1
    mv   s5, a7      # user a7

    mv a0, s0
    mv a1, s1
    mv a2, s2
    mv a3, s3
    mv a4, s4
    mv a5, s5
    call trap_handler

    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    sret

.balign 4
user_entry:
    li a0, 40
    li a1, 2
    li a7, 4          # SYS_ADD
    ecall             # a0 <- 42

    li a7, 1          # SYS_PUTCHAR
    ecall             # 打印 '*'

1:
    j 1b

.section .rodata
.balign 4
msg:
    .asciz "hello from user mode syscall\n"