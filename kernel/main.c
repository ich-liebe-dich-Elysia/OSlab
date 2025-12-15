#include "proc.h"
#include "riscv.h"

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
void test_all_basic_features(void);
void test_all_sync_primitives(void);
void test_all_process_management(void);
void scheduler(void);
void exit_process(int status);

// init 进程：运行所有测试
void init_process(void) {
  printf("\n========================================\n");
  printf("  OS Kernel Test Suite\n");
  printf("========================================\n");

  // 运行用户态系统调用测试
  extern void test_real_user_syscall(void);
  test_real_user_syscall();

  // 不应该执行到这里（test_real_user_syscall 会调用 exit）
  printf("\n[错误] init_process 不应该返回！\n");
  exit_process(0);
}

void kernel_main(void) {
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

  // 创建 init 进程来运行测试
  int pid = create_process(init_process, "init");
  if (pid < 0) {
    printf("Failed to create init process\n");
  } else {
    printf("Created init process (PID %d)\n", pid);
  }

  // 进入调度器
  scheduler();

  // 不应该到达这里
  while (1) {
    asm volatile("wfi");
  }
}
