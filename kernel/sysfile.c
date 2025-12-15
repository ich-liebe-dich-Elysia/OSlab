// 文件操作相关系统调用实现
// 简化版：只支持控制台I/O

#include "riscv.h"
#include "proc.h"
#include "trapframe.h"

typedef unsigned long uint64;

int printf(const char *fmt, ...);
extern int consolewrite(char *buf, int n);
extern int consoleread(char *buf, int n);

// 参数提取函数
extern int argint(int, int*);
extern int argaddr(int, uint64*);
extern int argstr(int, char*, int);

// 文件描述符定义
#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

// sys_open - 打开文件（简化：只支持控制台）
uint64 sys_open(void) {
  // 简化实现：总是返回stdout
  return FD_STDOUT;
}

// sys_close - 关闭文件
uint64 sys_close(void) {
  int fd;
  if (argint(0, &fd) < 0)
    return -1;
  
  // 简化实现：不关闭标准I/O
  if (fd <= FD_STDERR)
    return 0;
  
  return 0;
}

// sys_read - 从文件读取
uint64 sys_read(void) {
  int fd;
  uint64 addr;
  int n;
  
  if (argint(0, &fd) < 0 || argaddr(1, &addr) < 0 || argint(2, &n) < 0)
    return -1;
  
  if (n < 0)
    return -1;
  
  // 简化实现：只支持从stdin读取
  if (fd != FD_STDIN)
    return -1;
  
  char *buf = (char*)addr;
  return consoleread(buf, n);
}

// sys_write - 写入文件
uint64 sys_write(void) {
  int fd;
  uint64 addr;
  int n;
  
  if (argint(0, &fd) < 0 || argaddr(1, &addr) < 0 || argint(2, &n) < 0)
    return -1;
  
  if (n < 0)
    return -1;
  
  // 简化实现：只支持写到stdout/stderr
  if (fd != FD_STDOUT && fd != FD_STDERR)
    return -1;
  
  char *buf = (char*)addr;
  return consolewrite(buf, n);
}

// sys_sbrk - 调整堆大小
uint64 sys_sbrk(void) {
  int n;
  if (argint(0, &n) < 0)
    return -1;
  
  // 简化实现：暂不支持动态内存分配
  // TODO: 实现用户进程的堆管理
  return -1;
}

