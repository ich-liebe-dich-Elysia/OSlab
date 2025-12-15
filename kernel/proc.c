#include "proc.h"
#include "riscv.h"
#include "trapframe.h"

typedef unsigned long uint64;

struct cpu cpus[1];
struct proc proc[NPROC]; // 进程表

static uint64 pid_bitmap[(NPROC + 63) / 64]; // PID分配位图
static struct proc *runqueue[NPRIO];         // 优先级运行队列

// 按状态分类的进程队列
struct proc *sleepqueue = 0;  // 睡眠队列头
struct proc *zombiequeue = 0; // 僵尸队列头

// MLFQ参数
#define TIMESLICE_BASE 10   // 优先级0的时间片(ms)
#define BOOST_INTERVAL 1000 // 优先级提升周期(ms)
uint64 last_boost = 0;      // 上次优先级提升时间

extern void printf(const char *fmt, ...);
extern void *alloc_page(void);
extern void free_page(void *pa);
extern pagetable_t create_pagetable(void);
extern void destroy_pagetable(pagetable_t pagetable);
extern void swtch(struct context *old, struct context *new);
extern uint64 r_tp(void);

// 分配一个未使用的PID
static int allocpid(void) {
  for (int i = 0; i < NPROC; i++) {
    int word = i / 64;
    int bit = i % 64;
    if (!(pid_bitmap[word] & (1UL << bit))) {
      pid_bitmap[word] |= (1UL << bit);
      return i + 1;
    }
  }
  return -1;
}

// 释放PID使其可重用
static void freepid(int pid) {
  if (pid <= 0 || pid > NPROC)
    return;
  int i = pid - 1;
  int word = i / 64;
  int bit = i % 64;
  pid_bitmap[word] &= ~(1UL << bit);
}

// 初始化进程系统
void procinit(void) {
  for (int i = 0; i < NPROC; i++) {
    proc[i].state = UNUSED;
    proc[i].pid = 0;
    proc[i].kstack = 0;
  }
  for (int i = 0; i < NPRIO; i++) {
    runqueue[i] = 0;
  }
  for (int i = 0; i < (NPROC + 63) / 64; i++) {
    pid_bitmap[i] = 0;
  }
  // 初始化状态队列
  sleepqueue = 0;
  zombiequeue = 0;

  // 初始化第一个进程给主线程使用
  struct proc *p = &proc[0];
  p->state = RUNNING;
  p->pid = allocpid();
  p->priority = 0;
  p->timeslice = TIMESLICE_BASE;
  cpus[0].proc = p;
}

// ========== 状态队列辅助函数 ==========

// 将进程加入睡眠队列
static void sleep_enqueue(struct proc *p) {
  p->next = sleepqueue;
  sleepqueue = p;
}

// 将进程从睡眠队列移除
static void sleep_dequeue(struct proc *p) {
  if (sleepqueue == p) {
    sleepqueue = p->next;
    p->next = 0;
    return;
  }
  struct proc *prev = sleepqueue;
  while (prev && prev->next != p) {
    prev = prev->next;
  }
  if (prev) {
    prev->next = p->next;
    p->next = 0;
  }
}

// 将进程加入僵尸队列
static void zombie_enqueue(struct proc *p) {
  p->next = zombiequeue;
  zombiequeue = p;
}

// 将进程从僵尸队列移除
static void zombie_dequeue(struct proc *p) {
  if (zombiequeue == p) {
    zombiequeue = p->next;
    p->next = 0;
    return;
  }
  struct proc *prev = zombiequeue;
  while (prev && prev->next != p) {
    prev = prev->next;
  }
  if (prev) {
    prev->next = p->next;
    p->next = 0;
  }
}

int cpuid(void) { return 0; }

struct cpu *mycpu(void) { return &cpus[0]; }

struct proc *myproc(void) {
  struct cpu *c = mycpu();
  return c->proc;
}

struct proc *allocproc(void) {
  struct proc *p = 0;

  for (int i = 0; i < NPROC; i++) {
    if (proc[i].state == UNUSED) {
      p = &proc[i];
      break;
    }
  }

  if (p == 0)
    return 0;

  p->pid = allocpid();
  if (p->pid < 0) {
    return 0;
  }

  p->state = USED;
  p->killed = 0;
  p->priority = 0;               // MLFQ: 所有进程从最高优先级开始
  p->timeslice = TIMESLICE_BASE; // 初始时间片
  p->total_runtime = 0;
  p->chan = 0;
  p->xstate = 0;
  p->parent = 0;
  p->next = 0;

  void *kstack_page = alloc_page();
  if (kstack_page == 0) {
    freepid(p->pid);
    p->state = UNUSED;
    return 0;
  }
  p->kstack = (uint64)kstack_page + 4096;

  // 分配trapframe
  p->trapframe = (struct trapframe *)alloc_page();
  if (p->trapframe == 0) {
    free_page(kstack_page);
    freepid(p->pid);
    p->state = UNUSED;
    return 0;
  }

  p->pagetable = create_pagetable();
  if (p->pagetable == 0) {
    free_page((void*)p->trapframe);
    free_page(kstack_page);
    freepid(p->pid);
    p->state = UNUSED;
    return 0;
  }

  for (int i = 0; i < 16; i++)
    p->name[i] = 0;

  return p;
}

void freeproc(struct proc *p) {
  if (p->trapframe) {
    free_page((void*)p->trapframe);
    p->trapframe = 0;
  }
  if (p->kstack) {
    free_page((void *)(p->kstack - 4096));
    p->kstack = 0;
  }
  if (p->pagetable) {
    destroy_pagetable(p->pagetable);
    p->pagetable = 0;
  }
  freepid(p->pid);
  p->pid = 0;
  p->killed = 0;
  p->state = UNUSED;
  p->chan = 0;
  p->xstate = 0;
  p->parent = 0;
  p->next = 0;
}

static void enqueue(struct proc *p) {
  int prio = p->priority;
  if (prio < 0)
    prio = 0;
  if (prio >= NPRIO)
    prio = NPRIO - 1;

  p->next = 0;
  if (runqueue[prio] == 0) {
    runqueue[prio] = p;
  } else {
    struct proc *tail = runqueue[prio];
    while (tail->next)
      tail = tail->next;
    tail->next = p;
  }
}

static struct proc *dequeue(void) {
  for (int i = 0; i < NPRIO; i++) {
    if (runqueue[i]) {
      struct proc *p = runqueue[i];
      runqueue[i] = p->next;
      p->next = 0;
      return p;
    }
  }
  return 0;
}

int create_process(void (*entry)(void), const char *name) {
  struct proc *p = allocproc();
  if (p == 0)
    return -1;

  if (name) {
    for (int i = 0; i < 15 && name[i]; i++)
      p->name[i] = name[i];
  }

  // 设置父进程
  p->parent = myproc();

  // MLFQ: 进程已在 allocproc 中初始化为优先级0

  __builtin_memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)entry;
  p->context.sp = p->kstack;

  p->state = RUNNABLE;
  enqueue(p);

  return p->pid;
}

void exit_process(int status) {
  struct proc *p = myproc();
  if (p == 0)
    return;

  p->xstate = status;

  for (int i = 0; i < NPROC; i++) {
    if (proc[i].parent == p) {
      proc[i].parent = 0;
    }
  }

  p->state = ZOMBIE;
  zombie_enqueue(p); // 加入僵尸队列

  if (p->parent)
    wakeup(p->parent);

  sched();
}

int wait_process(int *status) {
  struct proc *p = myproc();
  if (p == 0)
    return -1;

  for (;;) {
    // 首先检查僵尸队列中的子进程
    struct proc *z = zombiequeue;
    while (z) {
      if (z->parent == p) {
        zombie_dequeue(z); // 从僵尸队列移除
        int pid = z->pid;
        if (status)
          *status = z->xstate;
        freeproc(z);
        return pid;
      }
      z = z->next;
    }

    // 检查是否有非僵尸状态的子进程
    int havekids = 0;
    for (int i = 0; i < NPROC; i++) {
      struct proc *child = &proc[i];
      if (child->parent == p && child->state != UNUSED &&
          child->state != ZOMBIE) {
        havekids = 1;
        break;
      }
    }

    if (!havekids)
      return -1;

    sleep(p);
  }
}

void scheduler(void) {
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;) {
    intr_on();

    struct proc *p = dequeue();
    if (p) {
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);
      c->proc = 0;
    } else {
      asm volatile("wfi");
    }
  }
}

void sched(void) {
  struct proc *p = myproc();
  if (p == 0)
    return;

  if (p->state == RUNNING)
    while (1)
      ;

  int intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

void yield(void) {
  struct proc *p = myproc();
  if (p == 0)
    return;

  p->state = RUNNABLE;
  enqueue(p);
  sched();
}

void sleep(void *chan) {
  struct proc *p = myproc();
  if (p == 0)
    return;

  p->chan = chan;
  p->state = SLEEPING;
  sleep_enqueue(p); // 加入睡眠队列
  sched();
}

void wakeup(void *chan) {
  // 只遍历睡眠队列，而非整个进程表
  struct proc *p = sleepqueue;
  while (p) {
    struct proc *next = p->next; // 先保存next，因为dequeue会修改它
    if (p->chan == chan) {
      sleep_dequeue(p); // 从睡眠队列移除
      p->chan = 0;
      p->state = RUNNABLE;

      // MLFQ: I/O密集型进程奖励，提升优先级
      if (p->priority > 0)
        p->priority--;

      // 重置时间片
      p->timeslice = TIMESLICE_BASE << p->priority;

      enqueue(p);
    }
    p = next;
  }
}

// MLFQ: 周期性提升所有进程优先级，防止饥饿
void boost_priority(void) {
  for (int i = 0; i < NPROC; i++) {
    struct proc *p = &proc[i];
    if (p->state != UNUSED && p->priority > 0) {
      // 从运行队列中移除（如果在队列中）
      if (p->state == RUNNABLE) {
        // 简化处理：直接重置优先级，不从旧队列移除
        // 因为 dequeue 会自然处理
        p->priority = 0;
        p->timeslice = TIMESLICE_BASE;
      } else if (p->state == RUNNING) {
        p->priority = 0;
        p->timeslice = TIMESLICE_BASE;
      }
    }
  }
}

// 打印队列统计信息（调试用）
void print_queue_stats(void) {
  int run_count[NPRIO] = {0};
  int sleep_count = 0;
  int zombie_count = 0;

  // 统计运行队列
  for (int i = 0; i < NPRIO; i++) {
    struct proc *p = runqueue[i];
    while (p) {
      run_count[i]++;
      p = p->next;
    }
  }

  // 统计睡眠队列
  struct proc *p = sleepqueue;
  while (p) {
    sleep_count++;
    p = p->next;
  }

  // 统计僵尸队列
  p = zombiequeue;
  while (p) {
    zombie_count++;
    p = p->next;
  }

  printf("\n=== Process Queue Stats ===\n");
  printf("Run Queue: ");
  for (int i = 0; i < NPRIO; i++) {
    printf("P%d=%d ", i, run_count[i]);
  }
  printf("\nSleep Queue: %d processes\n", sleep_count);
  printf("Zombie Queue: %d processes\n", zombie_count);
  printf("===========================\n");
}
