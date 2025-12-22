#ifndef RISCV_H
#define RISCV_H

// RISC-V CSR寄存器访问函数

// Machine Status Register
#define MSTATUS_MPP_MASK (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MIE (1L << 3)

static inline unsigned long r_mstatus() {
    unsigned long x;
    asm volatile("csrr %0, mstatus" : "=r" (x));
    return x;
}

static inline void w_mstatus(unsigned long x) {
    asm volatile("csrw mstatus, %0" : : "r" (x));
}

static inline void w_mepc(unsigned long x) {
    asm volatile("csrw mepc, %0" : : "r" (x));
}

// Machine Interrupt Enable
#define MIE_STIE (1L << 5)

static inline unsigned long r_mie() {
    unsigned long x;
    asm volatile("csrr %0, mie" : "=r" (x));
    return x;
}

static inline void w_mie(unsigned long x) {
    asm volatile("csrw mie, %0" : : "r" (x));
}

// Machine Exception/Interrupt Delegation
static inline void w_medeleg(unsigned long x) {
    asm volatile("csrw medeleg, %0" : : "r" (x));
}

static inline void w_mideleg(unsigned long x) {
    asm volatile("csrw mideleg, %0" : : "r" (x));
}

// Supervisor Status Register
#define SSTATUS_SPP (1L << 8)
#define SSTATUS_SPIE (1L << 5)
#define SSTATUS_SIE (1L << 1)

static inline unsigned long r_sstatus() {
    unsigned long x;
    asm volatile("csrr %0, sstatus" : "=r" (x));
    return x;
}

static inline void w_sstatus(unsigned long x) {
    asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9)
#define SIE_STIE (1L << 5)
#define SIE_SSIE (1L << 1)

static inline unsigned long r_sie() {
    unsigned long x;
    asm volatile("csrr %0, sie" : "=r" (x));
    return x;
}

static inline void w_sie(unsigned long x) {
    asm volatile("csrw sie, %0" : : "r" (x));
}

// Supervisor Trap Vector
static inline void w_stvec(unsigned long x) {
    asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline unsigned long r_stvec() {
    unsigned long x;
    asm volatile("csrr %0, stvec" : "=r" (x));
    return x;
}

// Supervisor Exception Program Counter
static inline unsigned long r_sepc() {
    unsigned long x;
    asm volatile("csrr %0, sepc" : "=r" (x));
    return x;
}

static inline void w_sepc(unsigned long x) {
    asm volatile("csrw sepc, %0" : : "r" (x));
}

// Supervisor Address Translation and Protection
static inline unsigned long r_satp() {
    unsigned long x;
    asm volatile("csrr %0, satp" : "=r" (x));
    return x;
}

static inline void w_satp(unsigned long x) {
    asm volatile("csrw satp, %0" : : "r" (x));
}

// Supervisor Cause Register
static inline unsigned long r_scause() {
    unsigned long x;
    asm volatile("csrr %0, scause" : "=r" (x));
    return x;
}

// Supervisor Trap Value
static inline unsigned long r_stval() {
    unsigned long x;
    asm volatile("csrr %0, stval" : "=r" (x));
    return x;
}

// Supervisor Scratch Register (用于用户态/内核态切换)
static inline unsigned long r_sscratch() {
    unsigned long x;
    asm volatile("csrr %0, sscratch" : "=r" (x));
    return x;
}

static inline void w_sscratch(unsigned long x) {
    asm volatile("csrw sscratch, %0" : : "r" (x));
}

// Timer registers
static inline unsigned long r_time() {
    unsigned long x;
    asm volatile("csrr %0, time" : "=r" (x));
    return x;
}

static inline void w_stimecmp(unsigned long x) {
    asm volatile("csrw 0x14d, %0" : : "r" (x));
}

static inline unsigned long r_stimecmp() {
    unsigned long x;
    asm volatile("csrr %0, 0x14d" : "=r" (x));
    return x;
}

// Machine Environment Configuration
static inline unsigned long r_menvcfg() {
    unsigned long x;
    asm volatile("csrr %0, 0x30a" : "=r" (x));
    return x;
}

static inline void w_menvcfg(unsigned long x) {
    asm volatile("csrw 0x30a, %0" : : "r" (x));
}

// Machine Counter Enable
static inline void w_mcounteren(unsigned long x) {
    asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline unsigned long r_mcounteren() {
    unsigned long x;
    asm volatile("csrr %0, mcounteren" : "=r" (x));
    return x;
}

// Physical Memory Protection
static inline void w_pmpaddr0(unsigned long x) {
    asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

static inline void w_pmpcfg0(unsigned long x) {
    asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

// Interrupt enable/disable
static inline void intr_on() {
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

static inline void intr_off() {
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

static inline int intr_get() {
    unsigned long x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

#endif

