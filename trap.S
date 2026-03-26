.section .text
.globl kernel_entry
.globl user_entry

kernel_entry:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)

    csrr s0, scause
    csrr s1, sepc
    csrr s2, stval

    mv a0, s0
    mv a1, s1
    mv a2, s2
    call trap_handler

1:
    j 1b

user_entry:
    la a0, user_msg
    call print_str

    ecall

2:
    j 2b

.section .rodata
user_msg:
    .asciz "hello from user mode\n"