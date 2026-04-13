.extern current

.section .text
.globl kernel_entry
.globl user_trap_entry
.globl user_entry
.globl user_entry2
.global user_return
.global kernel_return

.balign 4
user_trap_entry:
    # Trap came from U-mode.
    #
    # sscratch convention while user is running:
    #   sscratch = &current->scratch
    #
    # After csrrw:
    #   t0 = &current->scratch
    #   sscratch = user t0
    csrrw t0, sscratch, t0

    # Save user temporaries that would otherwise be clobbered early.
    sd t1, 0(t0)              # scratch.user_t1
    sd t2, 8(t0)              # scratch.user_t2

    # Save original user t0, then restore sscratch ASAP so nested S-traps
    # will still see a valid scratch pointer if they ever need it later.
    csrr t1, sscratch
    sd t1, 16(t0)             # scratch.user_t0
    csrw sscratch, t0

    # Save user sp into scratch before switching stacks.
    sd sp, 24(t0)             # scratch.user_sp

    # Load trapframe pointer and kernel stack top prepared by proc_init().
    ld t1, 32(t0)             # scratch.tf_ptr
    ld sp, 40(t0)             # scratch.kstack_top

    # We are now running in kernel context on kernel stack.
    # Any nested trap from here on must go directly to kernel_entry.
    la t2, kernel_entry
    csrw stvec, t2

    # Save user register context into trapframe.
    sd ra, 0(t1)

    ld t2, 24(t0)
    sd t2, 8(t1)              # tf->sp = saved user sp

    sd gp, 16(t1)
    sd tp, 24(t1)

    ld t2, 16(t0)
    sd t2, 32(t1)             # tf->t0 = saved user t0

    ld t2, 0(t0)
    sd t2, 40(t1)             # tf->t1 = saved user t1

    ld t2, 8(t0)
    sd t2, 48(t1)             # tf->t2 = saved user t2

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
kernel_entry:
    # Trap came from S-mode.
    #
    # Do NOT depend on sscratch or user-trap conventions here.
    # Just build a temporary trapframe directly on the current kernel stack.
    addi sp, sp, -256

    sd ra, 0(sp)

    # Save interrupted kernel sp = current sp + 256
    addi t0, sp, 256
    sd t0, 8(sp)

    sd gp, 16(sp)
    sd tp, 24(sp)

    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)

    sd s0, 56(sp)
    sd s1, 64(sp)
    sd s2, 72(sp)
    sd s3, 80(sp)
    sd s4, 88(sp)
    sd s5, 96(sp)
    sd s6, 104(sp)
    sd s7, 112(sp)
    sd s8, 120(sp)
    sd s9, 128(sp)
    sd s10, 136(sp)
    sd s11, 144(sp)

    sd a0, 152(sp)
    sd a1, 160(sp)
    sd a2, 168(sp)
    sd a3, 176(sp)
    sd a4, 184(sp)
    sd a5, 192(sp)
    sd a6, 200(sp)
    sd a7, 208(sp)

    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    csrr t0, sepc
    sd t0, 248(sp)

    # 把这块临时 trapframe 的地址传给 trap_handler
    # S-trap 不再关心 current->scratch，也不关心 sscratch 里有什么。
    mv a0, sp
    call trap_handler

    j kernel_return


.balign 4
user_return:
    # trap_handler may have switched current, so reload everything from current.
    la t0, current
    ld t0, 0(t0)              # t0 = current
    addi t1, t0, 48           # t1 = &current->tf   (scratch is first field)

    # While user runs:
    #   sscratch = &current->scratch
    csrw sscratch, t0

    # While user runs:
    #   traps must enter through user_trap_entry
    la t2, user_trap_entry
    csrw stvec, t2

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

    # Restore user t0/t1/t2 last.
    ld t0, 32(t1)
    ld t2, 48(t1)
    ld t1, 40(t1)

    sret


.balign 4
kernel_return:
    # Restore interrupted kernel context from temporary stack trapframe.

    ld t0, 248(sp)
    csrw sepc, t0

    # Keep sscratch pointing at current->scratch for the next user trap.
    # 虽然当前是从 S-trap 返回 S-mode，但你之后迟早还会回到用户态。
    la t3, current
    ld t3, 0(t3)
    csrw sscratch, t3

    # While staying in kernel:
    #   traps must enter through kernel_entry
    la t4, kernel_entry
    csrw stvec, t4

    ld ra, 0(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)

    ld s0, 56(sp)
    ld s1, 64(sp)
    ld s2, 72(sp)
    ld s3, 80(sp)
    ld s4, 88(sp)
    ld s5, 96(sp)
    ld s6, 104(sp)
    ld s7, 112(sp)
    ld s8, 120(sp)
    ld s9, 128(sp)
    ld s10, 136(sp)
    ld s11, 144(sp)

    ld a0, 152(sp)
    ld a1, 160(sp)
    ld a2, 168(sp)
    ld a3, 176(sp)
    ld a4, 184(sp)
    ld a5, 192(sp)
    ld a6, 200(sp)
    ld a7, 208(sp)

    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)

    # Restore t0/t1/t2 last.
    ld t2, 48(sp)
    ld t1, 40(sp)
    ld t0, 32(sp)

    addi sp, sp, 256
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