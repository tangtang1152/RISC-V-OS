.extern current

.section .text
.globl kernel_entry
.globl user_entry
.globl user_entry2

.balign 4
kernel_entry:
    csrw sscratch, sp

    la t0, current
    ld t0, 0(t0)          # t0 = current

    mv t2, t0             # t2 = &current->tf

    li t1, 4352           # sizeof(tf)=256, kstack=4096
    add sp, t0, t1        # sp = &current->kstack[KSTACK_SIZE]

    sd ra, 0(t2)

    csrr t1, sscratch
    sd t1, 8(t2)          # tf->sp

    sd gp, 16(t2)
    sd tp, 24(t2)

    sd t0, 32(t2)
    sd t1, 40(t2)
    sd t2, 48(t2)

    sd s0, 56(t2)
    sd s1, 64(t2)
    sd s2, 72(t2)
    sd s3, 80(t2)
    sd s4, 88(t2)
    sd s5, 96(t2)
    sd s6, 104(t2)
    sd s7, 112(t2)
    sd s8, 120(t2)
    sd s9, 128(t2)
    sd s10, 136(t2)
    sd s11, 144(t2)

    sd a0, 152(t2)
    sd a1, 160(t2)
    sd a2, 168(t2)
    sd a3, 176(t2)
    sd a4, 184(t2)
    sd a5, 192(t2)
    sd a6, 200(t2)
    sd a7, 208(t2)

    sd t3, 216(t2)
    sd t4, 224(t2)
    sd t5, 232(t2)
    sd t6, 240(t2)

    csrr t1, sepc
    sd t1, 248(t2)

    mv a0, t2
    call trap_handler

    la t0, current
    ld t0, 0(t0)
    mv t2, t0

    ld ra, 0(t2)

    ld gp, 16(t2)
    ld tp, 24(t2)

    ld s0, 56(t2)
    ld s1, 64(t2)
    ld s2, 72(t2)
    ld s3, 80(t2)
    ld s4, 88(t2)
    ld s5, 96(t2)
    ld s6, 104(t2)
    ld s7, 112(t2)
    ld s8, 120(t2)
    ld s9, 128(t2)
    ld s10, 136(t2)
    ld s11, 144(t2)

    ld a0, 152(t2)
    ld a1, 160(t2)
    ld a2, 168(t2)
    ld a3, 176(t2)
    ld a4, 184(t2)
    ld a5, 192(t2)
    ld a6, 200(t2)
    ld a7, 208(t2)

    ld t3, 216(t2)
    ld t4, 224(t2)
    ld t5, 232(t2)
    ld t6, 240(t2)

    ld t1, 248(t2)
    csrw sepc, t1

    ld t1, 8(t2)
    mv sp, t1

    # 最后再恢复 t0/t1/t2
    # 注意：此时恢复 t1 会覆盖刚才临时用的 t1，所以也必须放最后
    ld t0, 32(t2)
    ld t1, 40(t2)
    ld t2, 48(t2)
    
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