// M模式启动代码

#include "riscv.h"

// entry.S需要一个每个CPU的栈
__attribute__ ((aligned (16))) char stack0[4096];

// entry.S会跳转到这里（在M模式）
extern void main();

void timerinit();

void start()
{
    // 设置M模式的Previous Privilege为S模式
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // 设置mepc为main函数地址，mret会跳转到这里
    w_mepc((unsigned long)main);

    // 禁用分页（satp=0）
    asm volatile("csrw satp, zero");

    // 将所有中断和异常委托给S模式
    w_medeleg(0xffff);
    w_mideleg(0xffff);
    
    // 在sie中使能S模式的外部中断和定时器中断
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // 配置物理内存保护（PMP），给S模式访问所有物理内存的权限
    // 0x3fffffffffffffL是最大物理地址的右移2位
    w_pmpaddr0(0x3fffffffffffffL);
    w_pmpcfg0(0xf);  // R=1 W=1 X=1 A=01(TOR)

    // 初始化定时器中断
    timerinit();

    // 保持M模式的中断使能，以便接收定时器中断
    w_mie(r_mie() | MIE_STIE);

    // 通过mret切换到S模式并跳转到main
    asm volatile("mret");
}

// 设置下一次定时器中断
void timerinit()
{
    // 使能S模式访问time CSR
    w_mcounteren(r_mcounteren() | 2);
    
    // 使能Sstc扩展（允许S模式直接写stimecmp）
    w_menvcfg(r_menvcfg() | (1L << 63));
    
    // 设置第一次定时器中断（1秒后）
    // QEMU的时钟频率是10MHz
    w_stimecmp(r_time() + 10000000);
}
