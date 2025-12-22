// 真实用户态系统调用测试
// 在用户态运行代码，通过 ecall 触发真实的系统调用

#include "proc.h"
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

// 用户态测试程序（RISC-V 机器码）
// 测试系统调用：fork, getpid, uptime, sleep, wait, exit
uint32 user_program[] = {
    // ============ 测试 fork() ============
    0x00100893,  // li a7, 1        (SYS_fork)
    0x00000073,  // ecall
    // fork 返回后，a0 中是返回值（父进程中是子PID，子进程中是0）
    
    // ============ 检查是否是子进程 ============
    0x02050063,  // beqz a0, child  (如果 a0==0，跳转到子进程代码 +32字节)
    
    // ============ 父进程代码 ============
    // 调用 wait(0) 等待子进程
    0x00000513,  // li a0, 0        (status指针 = NULL)
    0x00300893,  // li a7, 3        (SYS_wait)
    0x00000073,  // ecall
    
    // 父进程 exit(0)
    0x00000513,  // li a0, 0
    0x00200893,  // li a7, 2        (SYS_exit)
    0x00000073,  // ecall
    0x0000006f,  // j .             (死循环)
    
    // ============ 子进程代码 (child:) ============
    // 调用 getpid() 获取自己的 PID
    0x00500893,  // li a7, 5        (SYS_getpid)
    0x00000073,  // ecall
    
    // 调用 uptime()
    0x00c00893,  // li a7, 12       (SYS_uptime)
    0x00000073,  // ecall
    
    // 子进程 sleep(10)
    0x00a00513,  // li a0, 10
    0x00b00893,  // li a7, 11       (SYS_sleep)
    0x00000073,  // ecall
    
    // 子进程 exit(0)
    0x00000513,  // li a0, 0
    0x00200893,  // li a7, 2        (SYS_exit)
    0x00000073,  // ecall
    0x0000006f,  // j .
};

/*
 * 用户态系统调用测试
 * 
 * 测试流程：
 * 1. 分配用户态内存页（代码页+栈页）
 * 2. 映射用户代码和栈到虚拟地址空间
 * 3. 设置 trapframe（PC、SP、寄存器）
 * 4. 通过 usertrapret() 切换到用户态（S-mode → U-mode）
 * 5. 用户程序执行 ecall 触发真实的系统调用
 * 
 * 测试的系统调用：
 * - fork()      : 创建子进程（无参数）
 * - getpid()    : 获取进程PID（无参数）
 * - uptime()    : 获取系统运行时间（无参数）
 * - sleep(n)    : 休眠指定ticks（1个参数）
 * - wait(status): 等待子进程（1个参数）
 * - exit(status): 退出进程（1个参数）
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
    printf("[成功] 分配用户代码页: 0x%x\n", (int)(uint64)ucode_page);
    
    // 分配用户栈页
    void *ustack_page = alloc_page();
    if (!ustack_page) {
        printf("[失败] 无法分配用户栈页\n");
        free_page(ucode_page);
        return;
    }
    printf("[成功] 分配用户栈页: 0x%x\n", (int)(uint64)ustack_page);
    
    // 复制用户程序代码到用户代码页
    uint32 *dst = (uint32 *)ucode_page;
    for (int i = 0; i < sizeof(user_program) / sizeof(uint32); i++) {
        dst[i] = user_program[i];
    }
    printf("[成功] 复制用户程序代码 (%d 字节)\n", (int)sizeof(user_program));
    
    // 映射用户代码页到虚拟地址 0x1000
    uint64 ucode_va = 0x1000;
    if (mappages(p->pagetable, ucode_va, 4096, (uint64)ucode_page, 
                 PTE_R | PTE_X | PTE_U) < 0) {
        printf("[失败] 无法映射用户代码页\n");
        free_page(ucode_page);
        free_page(ustack_page);
        return;
    }
    printf("[成功] 映射用户代码: VA=0x%x -> PA=0x%x\n", (int)ucode_va, (int)(uint64)ucode_page);
    
    // 映射用户栈页到虚拟地址 0x2000
    uint64 ustack_va = 0x2000;
    if (mappages(p->pagetable, ustack_va, 4096, (uint64)ustack_page,
                 PTE_R | PTE_W | PTE_U) < 0) {
        printf("[失败] 无法映射用户栈页\n");
        free_page(ucode_page);
        free_page(ustack_page);
        return;
    }
    printf("[成功] 映射用户栈: VA=0x%x -> PA=0x%x\n", (int)ustack_va, (int)(uint64)ustack_page);
    
    // 设置用户内存大小（0x3000 = 12KB，包含代码和栈）
    p->sz = 0x3000;
    
    // 在用户页表中映射内核空间（不设置 U 位，只允许 S-mode 访问）
    extern int map_kernel_to_user_pagetable(void *pt);
    if (map_kernel_to_user_pagetable(p->pagetable) < 0) {
        printf("[失败] 无法映射内核空间到用户页表\n");
        free_page(ucode_page);
        free_page(ustack_page);
        return;
    }
    printf("[成功] 已映射内核空间到用户页表\n");
    
    // 设置 trapframe 以便进入用户态
    p->trapframe->epc = ucode_va;           // 用户代码入口
    p->trapframe->sp = ustack_va + 4096;    // 用户栈顶
    
    // 清空所有通用寄存器
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
    printf("  - 用户PC: 0x%x\n", (int)p->trapframe->epc);
    printf("  - 用户SP: 0x%x\n", (int)p->trapframe->sp);
    printf("\n用户程序执行流程:\n");
    printf("  父进程: fork() -> wait() -> exit()\n");
    printf("  子进程: fork() -> getpid() -> uptime() -> sleep(10) -> exit()\n");
    printf("\n[执行] 跳转到用户态...\n");
    printf("========================================\n");
    
    // 进入用户态（这个函数不会返回，会通过 exit 系统调用退出）
    usertrapret();
}
