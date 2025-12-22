// 系统调用分发机制

#include "riscv.h"
#include "syscall.h"
#include "proc.h"
#include "trapframe.h"

typedef unsigned long uint64;

// 声明printf
int printf(const char *fmt, ...);

// 系统调用实现函数声明
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_kill(void);
extern uint64 sys_getpid(void);
extern uint64 sys_open(void);
extern uint64 sys_close(void);
extern uint64 sys_read(void);
extern uint64 sys_write(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);

// 系统调用函数指针表
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_kill]    sys_kill,
[SYS_getpid]  sys_getpid,
[SYS_open]    sys_open,
[SYS_close]   sys_close,
[SYS_read]    sys_read,
[SYS_write]   sys_write,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
};

// 系统调用名称（用于输出）
static char *syscall_names[] = {
[SYS_fork]    "fork",
[SYS_exit]    "exit",
[SYS_wait]    "wait",
[SYS_kill]    "kill",
[SYS_getpid]  "getpid",
[SYS_open]    "open",
[SYS_close]   "close",
[SYS_read]    "read",
[SYS_write]   "write",
[SYS_sbrk]    "sbrk",
[SYS_sleep]   "sleep",
[SYS_uptime]  "uptime",
};

// 从trapframe获取第n个系统调用参数（整数）
int argint(int n, int *ip) {
  struct proc *p = myproc();
  if (n < 0 || n >= 6)
    return -1;
  
  // RISC-V调用约定参数在a0-a5中
  switch (n) {
    case 0: *ip = p->trapframe->a0; break;
    case 1: *ip = p->trapframe->a1; break;
    case 2: *ip = p->trapframe->a2; break;
    case 3: *ip = p->trapframe->a3; break;
    case 4: *ip = p->trapframe->a4; break;
    case 5: *ip = p->trapframe->a5; break;
  }
  return 0;
}

// 获取第n个参数作为指针
int argaddr(int n, uint64 *ip) {
  struct proc *p = myproc();
  if (n < 0 || n >= 6)
    return -1;
  
  switch (n) {
    case 0: *ip = p->trapframe->a0; break;
    case 1: *ip = p->trapframe->a1; break;
    case 2: *ip = p->trapframe->a2; break;
    case 3: *ip = p->trapframe->a3; break;
    case 4: *ip = p->trapframe->a4; break;
    case 5: *ip = p->trapframe->a5; break;
  }
  return 0;
}

// 获取第n个参数作为字符串
int argstr(int n, char *buf, int max) {
  uint64 addr;
  if (argaddr(n, &addr) < 0)
    return -1;
  char *s = (char*)addr;
  int i;
  for (i = 0; i < max && s[i] != '\0'; i++) {
    buf[i] = s[i];
  }
  if (i >= max)
    return -1;
  buf[i] = '\0';
  return 0;
}

// 系统调用分发器
void syscall(void) {
  int num;
  struct proc *p = myproc();

  if (!p || !p->trapframe)
    return;

  num = p->trapframe->a7;  // 系统调用号在a7寄存器

  if (num > 0 && num < sizeof(syscalls)/sizeof(syscalls[0]) && syscalls[num]) {
    // 获取参数用于输出
    uint64 arg0 = p->trapframe->a0;
    
    // exit 不会返回，需要在调用前打印
    if (num == SYS_exit) {
      printf("[syscall] %s(%d) -> 进程退出\n", syscall_names[num], (int)arg0);
    }
    
    // 调用相应的系统调用处理函数，返回值存储在a0
    p->trapframe->a0 = syscalls[num]();
    
    // 输出其他系统调用结果
    if (num == SYS_sleep) {
      printf("[syscall] %s(%d) -> %d\n", syscall_names[num], (int)arg0, (int)p->trapframe->a0);
    } else if (num == SYS_wait) {
      printf("[syscall] %s(0x%x) -> %d\n", syscall_names[num], (int)arg0, (int)p->trapframe->a0);
    } else if (num != SYS_exit) {
      printf("[syscall] %s() -> %d\n", syscall_names[num], (int)p->trapframe->a0);
    }
  } else {
    // 无效的系统调用
    printf("[syscall] 未知系统调用 %d\n", num);
    p->trapframe->a0 = -1;
  }
}

