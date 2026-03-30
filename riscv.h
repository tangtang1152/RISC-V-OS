#ifndef RISCV_H
#define RISCV_H

static inline void w_stvec(void *x) {
    asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline void w_sepc(unsigned long x) {
    asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline unsigned long r_sepc(void) {
    unsigned long x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

static inline unsigned long r_sstatus(void) {
    unsigned long x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

static inline void w_sstatus(unsigned long x) {
    asm volatile("csrw sstatus, %0" : : "r"(x));
}

static inline void w_sp(unsigned long x){
    asm volatile("mv sp, %0" :: "r"(x));
}

static inline unsigned long r_sp(void){
    unsigned long x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

#endif