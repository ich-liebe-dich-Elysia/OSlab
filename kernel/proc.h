#ifndef PROC_H
#define PROC_H

#include "riscv.h"

#define NPROC 64 // 最大进程数
#define NPRIO 4  // 优先级数量 (0最高)

typedef unsigned long uint64;
typedef uint64 *pagetable_t;

// 上下文切换时保存的寄存器
struct context {
  uint64 ra;
  uint64 sp;
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// CPU状态
struct cpu {
  struct proc *proc;      // 当前运行的进程
  struct context context; // 调度器上下文
  int noff;               // push_off嵌套深度
  int intena;             // push_off前的中断状态
};

extern struct cpu cpus[1];

enum procstate { UNUSED, USED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };

// 进程控制块
struct proc {
  enum procstate state;         // 进程状态
  int pid;                      // 进程ID
  int killed;                   // 是否被kill
  int priority;                 // 当前优先级 (0最高, 3最低)
  uint64 timeslice;             // 当前时间片剩余
  uint64 total_runtime;         // 总运行时间
  uint64 kstack;                // 内核栈顶
  pagetable_t pagetable;        // 页表
  struct trapframe *trapframe;  // 陷阱帧（用于系统调用）
  struct context context;       // 调度上下文
  void *chan;                   // 睡眠通道
  int xstate;                   // 退出状态
  struct proc *parent;          // 父进程
  struct proc *next;            // 运行队列链表
  char name[16];                // 进程名
};

extern struct proc proc[NPROC];

// 按状态分类的进程队列
extern struct proc *sleepqueue;  // 睡眠队列头
extern struct proc *zombiequeue; // 僵尸队列头

void procinit(void);
struct proc *allocproc(void);
void freeproc(struct proc *p);
int create_process(void (*entry)(void), const char *name);
void exit_process(int status);
int wait_process(int *status);
void scheduler(void);
void sched(void);
void yield(void);
void sleep(void *chan);
void wakeup(void *chan);
struct proc *myproc(void);
struct cpu *mycpu(void);
int cpuid(void);
void boost_priority(void);
void print_queue_stats(void); // 打印队列统计信息

#endif
