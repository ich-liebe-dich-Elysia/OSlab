#include "riscv.h"
#include "proc.h"

// 类型定义
typedef unsigned long uint64;

// 函数声明
void uart_init(void);
void uart_enable_rx_interrupt(void);
int printf(const char *fmt, ...);
void buddy_init(void);
void kvminit(void);
void kvminithart(void);
void trap_init(void);
void trap_inithart(void);
void plic_init(void);
void plic_inithart(void);
uint64 get_ticks(void);
void test_process_creation(void);
void test_priority_scheduling(void);
void test_synchronization(void);


void kernel_main(void)
{
    uart_init();
    printf("\n=== OS Kernel Starting ===\n");
    
    buddy_init();
    printf("Physical memory initialized\n");
    
    kvminit();
    kvminithart();
    printf("Virtual memory enabled\n");
    
    trap_init();
    trap_inithart();
    plic_init();
    plic_inithart();
    printf("Interrupt system initialized\n");
    
    procinit();
    printf("Process system initialized\n");
    
    intr_on();
    
    test_process_creation();
    test_priority_scheduling();
    test_synchronization();
    
    printf("\n=== All tests completed ===\n");
    
    while (1) {
        asm volatile("wfi");
    }
}
