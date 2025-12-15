// 系统调用测试

#include "proc.h"
#include "trapframe.h"

int printf(const char *fmt, ...);
extern uint64 get_ticks(void);

// 模拟用户进程进行系统调用测试
void test_syscall_basic(void) {
  printf("\n[测试] 基本系统调用\n");
  
  struct proc *p = myproc();
  if (!p || !p->trapframe) {
    printf("[失败] 无法获取当前进程或trapframe\n");
    return;
  }
  
  // 测试 sys_getpid
  printf("测试 sys_getpid...\n");
  p->trapframe->a7 = 5; // SYS_getpid
  extern void syscall(void);
  syscall();
  int pid = p->trapframe->a0;
  if (pid == p->pid) {
    printf("[通过] sys_getpid 返回正确: %d\n", pid);
  } else {
    printf("[失败] sys_getpid 返回错误: got %d, expected %d\n", pid, p->pid);
  }
  
  // 测试 sys_uptime
  printf("测试 sys_uptime...\n");
  p->trapframe->a7 = 12; // SYS_uptime
  syscall();
  uint64 uptime = p->trapframe->a0;
  printf("[通过] sys_uptime 返回: %lu ticks\n", uptime);
  
  // 测试 sys_sleep
  printf("测试 sys_sleep (休眠10 ticks)...\n");
  uint64 before = get_ticks();
  p->trapframe->a7 = 11; // SYS_sleep
  p->trapframe->a0 = 10;  // 参数: 10 ticks
  syscall();
  uint64 after = get_ticks();
  if (after - before >= 10) {
    printf("[通过] sys_sleep 工作正常 (实际休眠 %lu ticks)\n", after - before);
  } else {
    printf("[警告] sys_sleep 可能未达到预期时间\n");
  }
  
  printf("[总结] 基本系统调用测试完成\n");
}

void test_syscall_fileio(void) {
  printf("\n[测试] 文件I/O系统调用\n");
  
  struct proc *p = myproc();
  if (!p || !p->trapframe) {
    printf("[失败] 无法获取当前进程或trapframe\n");
    return;
  }
  
  // 测试 sys_write
  printf("测试 sys_write...\n");
  char msg[] = "[sys_write输出] Hello from system call!\n";
  p->trapframe->a7 = 9;  // SYS_write
  p->trapframe->a0 = 1;  // stdout
  p->trapframe->a1 = (uint64)msg;  // 指针转换为地址
  p->trapframe->a2 = sizeof(msg) - 1;
  extern void syscall(void);
  syscall();
  int ret = p->trapframe->a0;
  if (ret > 0) {
    printf("[通过] sys_write 返回: %d 字节\n", ret);
  } else {
    printf("[失败] sys_write 返回错误: %d\n", ret);
  }
  
  printf("[总结] 文件I/O系统调用测试完成\n");
}

// 测试系统调用参数提取
void test_syscall_args(void) {
  printf("\n[测试] 系统调用参数提取\n");
  
  struct proc *p = myproc();
  if (!p || !p->trapframe) {
    printf("[失败] 无法获取当前进程或trapframe\n");
    return;
  }
  
  // 设置多个参数
  p->trapframe->a0 = 100;
  p->trapframe->a1 = 200;
  p->trapframe->a2 = 300;
  p->trapframe->a3 = 400;
  p->trapframe->a4 = 500;
  p->trapframe->a5 = 600;
  
  // 测试argint
  extern int argint(int, int*);
  int arg0, arg1, arg2;
  argint(0, &arg0);
  argint(1, &arg1);
  argint(2, &arg2);
  
  if (arg0 == 100 && arg1 == 200 && arg2 == 300) {
    printf("[通过] argint 正确提取参数: %d, %d, %d\n", arg0, arg1, arg2);
  } else {
    printf("[失败] argint 提取参数错误\n");
  }
  
  // 测试argaddr
  extern int argaddr(int, uint64*);
  uint64 addr0, addr1;
  argaddr(0, &addr0);
  argaddr(1, &addr1);
  
  if (addr0 == 100 && addr1 == 200) {
    printf("[通过] argaddr 正确提取地址: 0x%lx, 0x%lx\n", addr0, addr1);
  } else {
    printf("[失败] argaddr 提取地址错误\n");
  }
  
  printf("[总结] 参数提取测试完成\n");
}

// 运行所有系统调用测试
void test_all_syscalls(void) {
  printf("\n========================================\n");
  printf("  系统调用测试套件\n");
  printf("========================================\n");
  
  test_syscall_basic();
  test_syscall_fileio();
  test_syscall_args();
  
  printf("\n========================================\n");
  printf("  所有系统调用测试完成\n");
  printf("========================================\n");
}

