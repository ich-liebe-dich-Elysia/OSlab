// 真实用户态系统调用测试
// 在用户态运行代码，通过 ecall 触发真实的系统调用

#include "proc.h"
#include "riscv.h"
#include "trapframe.h"

typedef unsigned long uint64;
typedef unsigned int uint32;

int printf(const char *fmt, ...);
extern void *alloc_page(void);
extern void free_page(void *pa);
extern int mappages(uint64 *pagetable, uint64 va, uint64 size, uint64 pa, int perm);
extern void usertrapret(void);

// 页表项标志
#define PTE_V (1L << 0)
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)

// 简单的用户态程序（RISC-V 机器码）
// 这个程序会执行两个系统调用然后退出
uint32 user_program[] = {
    // getpid() 系统调用
    0x00500893,  // li a7, 5        (SYS_getpid)
    0x00000073,  // ecall
    
    // uptime() 系统调用
    0x00c00893,  // li a7, 12       (SYS_uptime)
    0x00000073,  // ecall
    
    // exit(0) 系统调用
    0x00000513,  // li a0, 0        (exit code = 0)
    0x00200893,  // li a7, 2        (SYS_exit)
    0x00000073,  // ecall
    
    // 如果 exit 失败，死循环
    0x0000006f,  // j .             (无限循环)
};

/*
 * 测试用户态系统调用
 * 
 * 这个函数会：
 * 1. 分配用户态内存页
 * 2. 映射用户代码和栈
 * 3. 设置 trapframe
 * 4. 通过 usertrapret() 进入用户态
 * 5. 用户程序执行 ecall 触发真实的系统调用
 */
void test_real_user_syscall(void) {
    printf("\n========================================\n");
    printf("  测试用户态系统调用\n");
    printf("========================================\n");
    
    struct proc *p = myproc();
    if (!p || !p->trapframe || !p->pagetable) {
        printf("[失败] 当前进程无效\n");
        return;
    }
    
    printf("[信息] 当前进程: pid=%d\n", p->pid);
    
    // 分配用户代码页
    void *ucode_page = alloc_page();
    if (!ucode_page) {
        printf("[失败] 无法分配用户代码页\n");
        return;
    }
    printf("[成功] 分配用户代码页: 0x%lx\n", (uint64)ucode_page);
    
    // 分配用户栈页
    void *ustack_page = alloc_page();
    if (!ustack_page) {
        printf("[失败] 无法分配用户栈页\n");
        free_page(ucode_page);
        return;
    }
    printf("[成功] 分配用户栈页: 0x%lx\n", (uint64)ustack_page);
    
    // 复制用户程序代码到用户代码页
    uint32 *dst = (uint32 *)ucode_page;
    for (int i = 0; i < sizeof(user_program) / sizeof(uint32); i++) {
        dst[i] = user_program[i];
    }
    printf("[成功] 复制用户程序代码 (%d 字节)\n", sizeof(user_program));
    
    // 映射用户代码页到虚拟地址 0x1000
    uint64 ucode_va = 0x1000;
    if (mappages(p->pagetable, ucode_va, 4096, (uint64)ucode_page, 
                 PTE_R | PTE_X | PTE_U) < 0) {
        printf("[失败] 无法映射用户代码页\n");
        free_page(ucode_page);
        free_page(ustack_page);
        return;
    }
    printf("[成功] 映射用户代码: VA=0x%lx -> PA=0x%lx\n", ucode_va, (uint64)ucode_page);
    
    // 映射用户栈页到虚拟地址 0x2000
    uint64 ustack_va = 0x2000;
    if (mappages(p->pagetable, ustack_va, 4096, (uint64)ustack_page,
                 PTE_R | PTE_W | PTE_U) < 0) {
        printf("[失败] 无法映射用户栈页\n");
        free_page(ucode_page);
        free_page(ustack_page);
        return;
    }
    printf("[成功] 映射用户栈: VA=0x%lx -> PA=0x%lx\n", ustack_va, (uint64)ustack_page);
    
    // 设置 trapframe 以便进入用户态
    p->trapframe->epc = ucode_va;           // 用户代码入口
    p->trapframe->sp = ustack_va + 4096;    // 用户栈顶
    
    // 清空所有通用寄存器（干净的初始状态）
    p->trapframe->ra = 0;
    p->trapframe->gp = 0;
    p->trapframe->tp = 0;
    p->trapframe->t0 = 0;
    p->trapframe->t1 = 0;
    p->trapframe->t2 = 0;
    p->trapframe->s0 = 0;
    p->trapframe->s1 = 0;
    p->trapframe->a0 = 0;
    p->trapframe->a1 = 0;
    p->trapframe->a2 = 0;
    p->trapframe->a3 = 0;
    p->trapframe->a4 = 0;
    p->trapframe->a5 = 0;
    p->trapframe->a6 = 0;
    p->trapframe->a7 = 0;
    
    printf("\n即将进入用户态\n");
    printf("  - 用户PC: 0x%lx\n", p->trapframe->epc);
    printf("  - 用户SP: 0x%lx\n", p->trapframe->sp);
    printf("  - 用户程序将执行:\n");
    printf("    1. getpid() -> 应返回 %d\n", p->pid);
    printf("    2. uptime() -> 应返回当前 ticks\n");
    printf("    3. exit(0)  -> 退出进程\n");
    printf("\n[执行] 跳转到用户态...\n\n");
    
    // 进入用户态！（这个函数不会返回，会通过 exit 系统调用退出）
    usertrapret();
    
    // 不应该执行到这里
    printf("[错误] usertrapret 返回了！\n");
}

