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
    mv   s3, a0       # 保存用户 a0
    mv   s4, a7       # 保存用户 a7

    mv a0, s0         # scause
    mv a1, s1         # sepc
    mv a2, s2         # stval
    mv a3, s3         # 用户 a0
    mv a4, s4         # 用户 a7
    # 调用 trap_handler
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
    li a7, 3          # syscall 3: 返回 'Z'
    ecall

    li a7, 1          # syscall 1: putchar(a0)
    ecall

1:
    j 1b

.section .rodata
.balign 4
msg:
    .asciz "hello from user mode syscall\n"