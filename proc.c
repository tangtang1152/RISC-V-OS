#include "proc.h"
#include "riscv.h"
#include "sbi.h"
#include "memlayout.h"
#include "vm.h"

extern void user_entry(void);
extern void user_entry2(void);
extern char __usertext_start[];

struct proc procs[PROC_NUM];
struct proc *current = 0;
static user_image_desc boot_images[PROC_NUM];

static const user_image_desc *proc_find_boot_image(int image_id) {
    if (image_id < 0 || image_id >= PROC_NUM) {
        return 0;
    }

    return &boot_images[image_id];
}

static int proc_validate_image(const user_image_desc *image) {
    if (!image) {
        return -1;
    }

    if (!image->name) {
        return -1;
    }

    switch (image->kind) {
        case USER_IMAGE_STATIC_LINKED:
            break;
        default:
            return -1;
    }

    if (!image->source_base) {
        return -1;
    }

    if (image->source_size == 0) {
        return -1;
    }

    if (image->source_size != image->layout.image_copy_size) {
        return -1;
    }

    if (image->layout.stack_top != USER_STACK_TOP) {
        return -1;
    }

    if (image->entry_offset >= image->layout.eager_map_size) {
        return -1;
    }

    return 0;
}
static void proc_init_user_context_from_image(struct proc *p,
                                              const user_image_desc *image) {
    if (!p || !image) {
        return;
    }

    p->tf.sp = image->layout.stack_top;
    p->tf.sepc = USER_TEXT_BASE + image->entry_offset;
}
static int proc_load_image(struct proc *p, const user_image_desc *image) {
    if (!p) {
        return -1;
    }

    if (proc_validate_image(image) < 0) {
        return -1;
    }

    if (!p->space) {
        return -1;
    }

    p->user_pagetable = vm_make_user_pagetable(p->space, image);
    if (!p->user_pagetable) {
        return -1;
    }

    proc_init_user_context_from_image(p, image);
    return 0;
}

static void proc_basic_init(struct proc *p, int pid) {
    p->pid = pid;
    p->state = PROC_RUNNABLE;
    p->block_reason = PROC_BLOCK_NONE;

    p->wakeup_tick = 0;

    p->wait_pid = -1;
    p->waited_by = -1;

    p->exit_code = 0;
    p->wait_status_uaddr = 0;

    p->space = vm_space_for_pid(pid);
    p->user_pagetable = 0;

    p->scratch.user_t0 = 0;
    p->scratch.user_t1 = 0;
    p->scratch.user_t2 = 0;
    p->scratch.user_sp = 0;
    p->scratch.tf_ptr = (unsigned long)&(p->tf);
    p->scratch.kstack_top = (unsigned long)(p->kstack + KSTACK_SIZE);

    p->tf.ra = 0;
    p->tf.sp = USER_STACK_TOP;
    p->tf.gp = 0;
    p->tf.tp = 0;

    p->tf.t0 = 0;
    p->tf.t1 = 0;
    p->tf.t2 = 0;

    p->tf.s0 = 0;
    p->tf.s1 = 0;
    p->tf.s2 = 0;
    p->tf.s3 = 0;
    p->tf.s4 = 0;
    p->tf.s5 = 0;
    p->tf.s6 = 0;
    p->tf.s7 = 0;
    p->tf.s8 = 0;
    p->tf.s9 = 0;
    p->tf.s10 = 0;
    p->tf.s11 = 0;

    p->tf.a0 = 0;
    p->tf.a1 = 0;
    p->tf.a2 = 0;
    p->tf.a3 = 0;
    p->tf.a4 = 0;
    p->tf.a5 = 0;
    p->tf.a6 = 0;
    p->tf.a7 = 0;

    p->tf.t3 = 0;
    p->tf.t4 = 0;
    p->tf.t5 = 0;
    p->tf.t6 = 0;

    p->tf.sepc = 0;
}

static int proc_create_with_image(struct proc *p, int pid, const user_image_desc *image) {
    if (!p) {
        return -1;
    }

    proc_basic_init(p, pid);

    if (proc_load_image(p, image) < 0) {
        return -1;
    }

    return 0;
}

void proc_init(void) {
    vm_build_static_user_image_desc(&boot_images[0],
                                    "user0",
                                    (unsigned long)user_entry - (unsigned long)__usertext_start);

    vm_build_static_user_image_desc(&boot_images[1],
                                    "user1",
                                    (unsigned long)user_entry2 - (unsigned long)__usertext_start);

    if (proc_create_with_image(&procs[0], 0, &boot_images[0]) < 0) {
        print_str("[KERNEL] failed to create boot proc user0\n");
        while (1) {}
    }

    if (proc_create_with_image(&procs[1], 1, &boot_images[1]) < 0) {
        print_str("[KERNEL] failed to create boot proc user1\n");
        while (1) {}
    }

    current = &procs[0];
    current->state = PROC_RUNNING;
}

int proc_switch(void) {
    int start = current ? current->pid : 0;

    for (int i = 1; i <= PROC_NUM; i++) {
        int next = (start + i) % PROC_NUM;

        if (procs[next].state == PROC_RUNNABLE) {
            current = &procs[next];
            current->state = PROC_RUNNING;
            return 0;
        }
    }
    return -1;
}

void schedule(void) {
    int idle_printed = 0;

    while (1) {
        if (proc_switch() == 0) {
            return;
        }

        if (!idle_printed) {
            print_str("[KERNEL] schedule: no runnable process, wait for interrupt\n");
            idle_printed = 1;
        }

        // 关键：idle 等中断前开 SIE，否则可能永远等不到 timer
        unsigned long s = r_sstatus();
        w_sstatus(s | (1UL << 1));   // SIE=1
        // S-mode中断还是依赖U-mode的scratch来保存当前进程的上下文 
        w_sscratch((unsigned long)&current->scratch);
        asm volatile("wfi");
        w_sstatus(s);                // 恢复原值
    }
}

void proc_wakeup_sleepers(unsigned long now) {
    for (int i = 0; i < PROC_NUM; i++) {
        if (procs[i].state == PROC_BLOCKED &&
            procs[i].block_reason == PROC_BLOCK_SLEEP &&
            procs[i].wakeup_tick <= now) {
            procs[i].block_reason = PROC_BLOCK_NONE;
            procs[i].state = PROC_RUNNABLE;
        }
    }
}
void proc_wakeup_waiters(int exited_pid) {
    int waiter_pid;

    print_str("[KERNEL] wake_waiters: exited_pid=");
    print_hex((unsigned long)exited_pid);
    print_str("\n");

    if (exited_pid < 0 || exited_pid >= PROC_NUM) {
        print_str("[KERNEL] wake_waiters: invalid exited pid\n");
        return;
    }

    waiter_pid = procs[exited_pid].waited_by;

    print_str("[KERNEL] wake_waiters: waited_by=");
    print_hex((unsigned long)waiter_pid);
    print_str("\n");

    if (waiter_pid < 0 || waiter_pid >= PROC_NUM) {
        print_str("[KERNEL] wake_waiters: no valid waiter\n");
        return;
    }

    print_str("[KERNEL] wake_waiters: waiter state=");
    print_str(proc_state_name(procs[waiter_pid].state));
    print_str(" reason=");
    print_str(proc_block_reason_name(procs[waiter_pid].block_reason));
    print_str(" wait_pid=");
    print_hex((unsigned long)procs[waiter_pid].wait_pid);
    print_str("\n");

    if (procs[waiter_pid].state == PROC_BLOCKED &&
        procs[waiter_pid].block_reason == PROC_BLOCK_WAIT &&
        procs[waiter_pid].wait_pid == exited_pid) {
        procs[waiter_pid].block_reason = PROC_BLOCK_NONE;
        procs[waiter_pid].state = PROC_RUNNABLE;

        print_str("[KERNEL] wake_waiters: waiter -> RUNNABLE\n");
    }
}

void proc_reap(int pid) {
    if (pid < 0 || pid >= PROC_NUM) {
        return;
    }

    procs[pid].state = PROC_UNUSED;
    procs[pid].block_reason = PROC_BLOCK_NONE;
    procs[pid].waited_by = -1;
    procs[pid].wait_pid = -1;
    procs[pid].wait_status_uaddr = 0;
    procs[pid].wakeup_tick = 0;
}

const char *proc_state_name(int state) {
    switch (state) {
        case PROC_UNUSED:   return "UNUSED";
        case PROC_RUNNABLE: return "RUNNABLE";
        case PROC_RUNNING:  return "RUNNING";
        case PROC_BLOCKED:  return "BLOCKED";
        case PROC_ZOMBIE:   return "ZOMBIE";
        default:            return "UNKNOWN";
    }
}
const char *proc_block_reason_name(int reason) {
    switch (reason) {
        case PROC_BLOCK_NONE:  return "NONE";
        case PROC_BLOCK_SLEEP: return "SLEEP";
        case PROC_BLOCK_WAIT:  return "WAIT";
        default:               return "UNKNOWN";
    }
}

void proc_dump(void) {
    print_str("[KERNEL] proc dump begin\n");

    for (int i = 0; i < PROC_NUM; i++) {
        print_str("  pid=");
        print_hex((unsigned long)procs[i].pid);
        print_str(" state=");
        print_str(proc_state_name(procs[i].state));
        print_str(" reason=");
        print_str(proc_block_reason_name(procs[i].block_reason));
        print_str(" sepc=");
        print_hex(procs[i].tf.sepc);
        print_str(" sp=");
        print_hex(procs[i].tf.sp);
        print_str("\n");
    }

    print_str("[KERNEL] proc dump end\n");
}

const user_image_desc *proc_find_boot_image_by_id(int image_id) {
    return proc_find_boot_image(image_id);
}

/*
 * proc_exec_current_image: replace current process's address space and
 * execution context with a new boot image. Used by sys_exec.
 *
 * vm_make_user_pagetable internally calls vm_space_reset (free old pages
 * via kfree_page) and vm_space_alloc_page (allocate new pages via
 * kalloc_page).  With FIFO kalloc, freed pages go to the tail and are
 * not immediately re-allocated, so the active SATP stays valid during
 * the build.
 *
 * After building the new pagetable, we set sepc and sp to the new
 * image's entry point / stack top.  The trap handler will use these
 * values on return to user mode.
 *
 * Returns 0 on success, -1 on failure (current process unchanged).
 */
int proc_exec_current_image(int image_id) {
    const user_image_desc *image;
    pagetable_t new_pt;

    if (!current) {
        return -1;
    }

    image = proc_find_boot_image(image_id);
    if (!image) {
        print_str("[KERNEL] exec: image not found, id=");
        print_hex((unsigned long)image_id);
        print_str("\n");
        return -1;
    }

    if (proc_validate_image(image) < 0) {
        print_str("[KERNEL] exec: invalid image\n");
        return -1;
    }

    /* vm_make_user_pagetable frees old pages and builds new pagetable.
     * With FIFO kalloc, old freed pages are not immediately reused. */
    new_pt = vm_make_user_pagetable(current->space, image);
    if (!new_pt) {
        print_str("[KERNEL] exec: failed to rebuild page table\n");
        return -1;
    }

    current->user_pagetable = new_pt;

    /* Set new entry point and stack. */
    current->tf.sepc = USER_TEXT_BASE + image->entry_offset;
    current->tf.sp = image->layout.stack_top;

    print_str("[KERNEL] exec: pid=");
    print_hex((unsigned long)current->pid);
    print_str(" -> image=");
    if (image->name) {
        print_str(image->name);
    } else {
        print_hex((unsigned long)image_id);
    }
    print_str(" sepc=");
    print_hex(current->tf.sepc);
    print_str(" sp=");
    print_hex(current->tf.sp);
    print_str("\n");

    return 0;
}
