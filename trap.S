.extern current

.section .text
.globl kernel_entry
.globl user_entry

.balign 4
kernel_entry:
    csrw sscratch, sp

    la t0, current        # la rd, label ; t0 = &current
    ld t0, 0(t0)          # t0 = *(&current) = current = struct proc *
    
    mv t2, t0             # t2 = &current->tf

    li t1, 4272        # 4272 = 4096 + 176
    add sp, t0, t1     # sp = &current->kstack[KSTACK_SIZE]

    sd ra, 0(t2)

    csrr t1, sscratch
    sd t1, 8(t2)

    sd s0, 16(t2)
    sd s1, 24(t2)
    sd s2, 32(t2)
    sd s3, 40(t2)
    sd s4, 48(t2)
    sd s5, 56(t2)
    sd s6, 64(t2)
    sd s7, 72(t2)
    sd s8, 80(t2)
    sd s9, 88(t2)
    sd s10, 96(t2)
    sd s11, 104(t2)

    sd a0, 112(t2)
    sd a1, 120(t2)
    sd a2, 128(t2)
    sd a3, 136(t2)
    sd a4, 144(t2)
    sd a5, 152(t2)
    sd a6, 160(t2)
    sd a7, 168(t2)

    mv a0, t2          # a0 = trap_frame*
    call trap_handler

    ld ra, 0(t2)

    ld s0, 16(t2)
    ld s1, 24(t2)
    ld s2, 32(t2)
    ld s3, 40(t2)
    ld s4, 48(t2)
    ld s5, 56(t2)
    ld s6, 64(t2)
    ld s7, 72(t2)
    ld s8, 80(t2)
    ld s9, 88(t2)
    ld s10, 96(t2)
    ld s11, 104(t2)

    ld a0, 112(t2)
    ld a1, 120(t2)
    ld a2, 128(t2)
    ld a3, 136(t2)
    ld a4, 144(t2)
    ld a5, 152(t2)
    ld a6, 160(t2)
    ld a7, 168(t2)

    # 最后再恢复 user sp
    ld t1, 8(t2)
    mv sp, t1

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