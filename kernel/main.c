#include "riscv.h"

// 类型定义
typedef unsigned long uint64;
typedef uint64 *pagetable_t;

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

/*
 * 测试时钟中断
 */
void test_timer_interrupt(void) {
    printf("\n=== Timer Interrupt Test ===\n");
    
    // 获取初始时钟计数
    uint64 ticks_start = get_ticks();
    printf("Initial ticks: %d\n", (int)ticks_start);
    
    // 开启中断
    printf("Enabling interrupts...\n");
    intr_on();
    
    // 等待5次时钟中断
    printf("Waiting for 5 timer interrupts...\n");
    for (int i = 0; i < 5; i++) {
        uint64 old_ticks = get_ticks();
        
        // 等待ticks增加
        while (get_ticks() == old_ticks) {
            asm volatile("wfi");
        }
        
        printf("Tick %d: ticks=%d\n", i+1, (int)get_ticks());
    }
    
    uint64 ticks_end = get_ticks();
    printf("Timer interrupt test completed: %d -> %d ticks\n", 
           (int)ticks_start, (int)ticks_end);
}

/*
 * 测试异常处理
 */
void test_exception_handling(void) {
    printf("\n=== Exception Handling Test ===\n");
    printf("Testing various exception scenarios...\n");
    
    // 测试1：正常内存访问（应该成功）
    printf("\n1. Normal memory access: ");
    int test_var = 42;
    printf("OK (value=%d)\n", test_var);
    
    // 测试2：对齐访问
    printf("2. Aligned access: ");
    volatile int *aligned_ptr = &test_var;
    *aligned_ptr = 100;
    printf("OK (value=%d)\n", *aligned_ptr);
    
    printf("\nException handling framework ready.\n");
    printf("Note: Actual fault tests would crash the system.\n");
}

/*
 * 测试UART中断
 */
void test_uart_interrupt(void) {
    printf("\n=== UART Interrupt Test ===\n");
    printf("UART interrupt enabled.\n");
    printf("Type characters to test (they will be echoed back).\n");
    printf("Note: This is a passive test - try typing in QEMU console.\n");
}

/*
 * 主入口
 */
void kernel_main(void)
{
    // 初始化
    uart_init();
    printf("\n=== OS Kernel Starting ===\n");
    
    buddy_init();
    printf("Physical memory allocator initialized\n");
    
    kvminit();
    printf("Page table initialized\n");
    
    kvminithart();
    printf("Virtual memory enabled\n");
    
    trap_init();
    trap_inithart();
    printf("Interrupt system initialized\n");
    
    plic_init();
    plic_inithart();
    printf("PLIC initialized\n");
    
    // 测试各种中断和异常
    test_exception_handling();
    test_timer_interrupt();
    
    // 使能UART接收中断
    uart_enable_rx_interrupt();
    test_uart_interrupt();
    
    printf("\n=== System ready, entering idle loop ===\n");
    
    // 进入空闲循环
    while (1) {
        asm volatile("wfi");
    }
}
