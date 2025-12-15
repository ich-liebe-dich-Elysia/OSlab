// 进程相关系统调用实现

#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "trapframe.h"

typedef unsigned long uint64;

int printf(const char *fmt, ...);
extern uint64 get_ticks(void);

// 参数提取函数
extern int argint(int, int*);
extern int argaddr(int, uint64*);

// sys_fork - 创建子进程
uint64 sys_fork(void) {
  // TODO: 实现fork（需要复制页表等）
  return -1;  // 暂未实现
}

// sys_exit - 终止当前进程
uint64 sys_exit(void) {
  int status;
  if (argint(0, &status) < 0)
    return -1;
  
  exit_process(status);
  return 0;  // 不会执行到这里
}

// sys_wait - 等待子进程
uint64 sys_wait(void) {
  uint64 addr;
  if (argaddr(0, &addr) < 0)
    return -1;
  
  int status;
  int pid = wait_process(&status);
  
  // 将status写回用户空间（简化版）
  if (addr != 0) {
    int *p = (int*)addr;
    *p = status;
  }
  
  return pid;
}

// sys_kill - 发送信号给进程（简化：直接终止）
uint64 sys_kill(void) {
  int pid;
  if (argint(0, &pid) < 0)
    return -1;
  
  // 简化实现：查找进程并标记为killed
  extern struct proc proc[];
  
  for (int i = 0; i < NPROC; i++) {
    if (proc[i].pid == pid) {
      // 简单地让进程在下次调度时退出
      proc[i].killed = 1;
      if (proc[i].state == SLEEPING) {
        proc[i].state = RUNNABLE;
      }
      return 0;
    }
  }
  
  return -1;  // 进程不存在
}

// sys_getpid - 获取当前进程PID
uint64 sys_getpid(void) {
  struct proc *p = myproc();
  if (!p)
    return -1;
  return p->pid;
}

// sys_sleep - 休眠指定ticks数
uint64 sys_sleep(void) {
  int n;
  if (argint(0, &n) < 0)
    return -1;
  
  uint64 start = get_ticks();
  while (get_ticks() - start < n) {
    // 检查是否被killed
    if (myproc()->killed) {
      return -1;
    }
    yield();
  }
  
  return 0;
}

// sys_uptime - 获取系统运行时间
uint64 sys_uptime(void) {
  return get_ticks();
}

