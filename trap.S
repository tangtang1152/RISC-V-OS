.extern current

.section .text
.globl kernel_entry
.globl user_entry
.globl user_entry2

.balign 4
kernel_entry:
    # save user sp first
    csrw sscratch, sp

    # t0 = current
    la t0, current
    ld t0, 0(t0)

    # t2 = &current->tf
    mv t2, t0

    # sp = &current->kstack[KSTACK_SIZE]
    # 4328 = sizeof(trap_frame)(232) + KSTACK_SIZE(4096)
    li t1, 4328
    add sp, t0, t1

    # save user context into current->tf
    sd ra, 0(t2)

    csrr t1, sscratch
    sd t1, 8(t2)      # tf->sp = user sp

    sd gp, 16(t2)
    sd tp, 24(t2)

    sd s0, 32(t2)
    sd s1, 40(t2)
    sd s2, 48(t2)
    sd s3, 56(t2)
    sd s4, 64(t2)
    sd s5, 72(t2)
    sd s6, 80(t2)
    sd s7, 88(t2)
    sd s8, 96(t2)
    sd s9, 104(t2)
    sd s10, 112(t2)
    sd s11, 120(t2)

    sd a0, 128(t2)
    sd a1, 136(t2)
    sd a2, 144(t2)
    sd a3, 152(t2)
    sd a4, 160(t2)
    sd a5, 168(t2)
    sd a6, 176(t2)
    sd a7, 184(t2)

    sd t3, 192(t2)
    sd t4, 200(t2)
    sd t5, 208(t2)
    sd t6, 216(t2)

    csrr t1, sepc
    sd t1, 224(t2) 

    # a0 = trap_frame *
    mv a0, t2
    call trap_handler

    # t registers are caller-saved, so recompute current->tf after call
    la t0, current
    ld t0, 0(t0)
    mv t2, t0

    # restore user context from current->tf
    ld ra, 0(t2)

    ld gp, 16(t2)
    ld tp, 24(t2)

    ld s0, 32(t2)
    ld s1, 40(t2)
    ld s2, 48(t2)
    ld s3, 56(t2)
    ld s4, 64(t2)
    ld s5, 72(t2)
    ld s6, 80(t2)
    ld s7, 88(t2)
    ld s8, 96(t2)
    ld s9, 104(t2)
    ld s10, 112(t2)
    ld s11, 120(t2)

    ld a0, 128(t2)
    ld a1, 136(t2)
    ld a2, 144(t2)
    ld a3, 152(t2)
    ld a4, 160(t2)
    ld a5, 168(t2)
    ld a6, 176(t2)
    ld a7, 184(t2)

    ld t3, 192(t2)
    ld t4, 200(t2)
    ld t5, 208(t2)
    ld t6, 216(t2)

    # restore user sp last
    ld t1, 8(t2)
    mv sp, t1

    ld t1, 224(t2)
    csrw sepc, t1

    sret

.balign 4
user_entry:
    call user_main
1:
    j 1b

.balign 4
user_entry2:
    call user_main2
2:
    j 2b