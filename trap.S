.extern current

.section .text
.globl kernel_entry
.globl user_entry
.globl user_entry2
.global user_return

.balign 4
kernel_entry:
    # swap: t0 <- sscratch(scratch_ptr), sscratch <- user_t0
    csrrw t0, sscratch, t0

    # early scratch save of user t1/t2/t0/sp
    sd t1, 0(t0)              # scratch.user_t1
    sd t2, 8(t0)              # scratch.user_t2

    csrr t1, sscratch         # t1 = user_t0
    sd t1, 16(t0)             # scratch.user_t0

    sd sp, 24(t0)             # scratch.user_sp

    # load pointers prepared in proc_init()
    ld t1, 32(t0)             # scratch.tf_ptr
    ld sp, 40(t0)             # scratch.kstack_top

    # t1 = trap_frame *
    # t0 = scratch *
    sd ra, 0(t1)

    ld t2, 24(t0)
    sd t2, 8(t1)              # tf->sp = user sp

    sd gp, 16(t1)
    sd tp, 24(t1)

    ld t2, 16(t0)
    sd t2, 32(t1)             # tf->t0 = saved user_t0

    ld t2, 0(t0)
    sd t2, 40(t1)             # tf->t1 = saved user_t1

    ld t2, 8(t0)
    sd t2, 48(t1)             # tf->t2 = saved user_t2

    sd s0, 56(t1)
    sd s1, 64(t1)
    sd s2, 72(t1)
    sd s3, 80(t1)
    sd s4, 88(t1)
    sd s5, 96(t1)
    sd s6, 104(t1)
    sd s7, 112(t1)
    sd s8, 120(t1)
    sd s9, 128(t1)
    sd s10, 136(t1)
    sd s11, 144(t1)

    sd a0, 152(t1)
    sd a1, 160(t1)
    sd a2, 168(t1)
    sd a3, 176(t1)
    sd a4, 184(t1)
    sd a5, 192(t1)
    sd a6, 200(t1)
    sd a7, 208(t1)

    sd t3, 216(t1)
    sd t4, 224(t1)
    sd t5, 232(t1)
    sd t6, 240(t1)

    csrr t2, sepc
    sd t2, 248(t1)

    mv a0, t1
    call trap_handler
    j user_return

.balign 4
user_return:
    # after handler, reload current scratch/tf because current may have changed
    la t0, current
    ld t0, 0(t0)              # t0 = current
    addi t0, t0, 0            # t0 = &current->scratch
    addi t1, t0, 48           # t1 = &current->tf

    # while back in user mode, sscratch must point to current scratch
    csrw sscratch, t0

    ld ra, 0(t1)

    ld gp, 16(t1)
    ld tp, 24(t1)

    ld s0, 56(t1)
    ld s1, 64(t1)
    ld s2, 72(t1)
    ld s3, 80(t1)
    ld s4, 88(t1)
    ld s5, 96(t1)
    ld s6, 104(t1)
    ld s7, 112(t1)
    ld s8, 120(t1)
    ld s9, 128(t1)
    ld s10, 136(t1)
    ld s11, 144(t1)

    ld a0, 152(t1)
    ld a1, 160(t1)
    ld a2, 168(t1)
    ld a3, 176(t1)
    ld a4, 184(t1)
    ld a5, 192(t1)
    ld a6, 200(t1)
    ld a7, 208(t1)

    ld t3, 216(t1)
    ld t4, 224(t1)
    ld t5, 232(t1)
    ld t6, 240(t1)

    ld t2, 248(t1)
    csrw sepc, t2

    ld t2, 8(t1)
    mv sp, t2                 # restore user sp

    # restore user t0/t1/t2 last
    ld t0, 32(t1)
    ld t2, 48(t1)
    ld t1, 40(t1)

    sret

.section .usertext
.balign 4096
.globl user_entry
.globl user_entry2

user_entry:
    call user_main
1:
    j 1b

.balign 4096
user_entry2:
    call user_main2
2:
    j 2b