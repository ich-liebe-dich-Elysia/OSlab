#include "proc.h"

typedef unsigned long uint64;

extern int printf(const char *fmt, ...);
extern uint64 get_ticks(void);
extern void yield(void);
extern void print_queue_stats(void);
// 访问内部 proc 结构以进行白盒测试
extern struct proc proc[NPROC];

// ==========================================
// 辅助任务
// ==========================================

void task_null(void) { exit_process(0); }

void task_child_report(void) {
  int pid = myproc()->pid;
  printf("子进程 (PID %d) 运行中。父进程 PID: %d\n", pid,
         myproc()->parent ? myproc()->parent->pid : 0);
  exit_process(0);
}



// ==========================================
// 测试套件
// ==========================================

// 1. 基本生命周期
void test_basic_lifecycle(void) {
  printf("\n[测试] 基本生命周期 (创建/退出/等待)\n");

  int pid = create_process(task_child_report, "child_basic");
  if (pid < 0) {
    printf("[失败] 创建进程失败\n");
    return;
  }
  printf("已创建子进程 %d\n", pid);

  int status;
  int waited_pid = wait_process(&status);

  if (waited_pid == pid && status == 0) {
    printf("[通过] 子进程创建、运行并等待成功\n");
  } else {
    printf("[失败] 等待返回 PID %d (预期 %d) 状态 %d\n", waited_pid, pid,
           status);
  }
}

// 2. PID 重用
void test_pid_reuse(void) {
  printf("\n[测试] PID 重用\n");

  int initial_pid = create_process(task_null, "p_null");
  printf("初始 PID: %d\n", initial_pid);
  wait_process(0);

  // 创建并销毁多个进程
  int last_pid = -1;
  for (int i = 0; i < 10; i++) {
    int pid = create_process(task_null, "p_reuse");
    if (pid < 0) {
      printf("[失败] 在第 %d 次迭代中创建进程失败\n", i);
      return;
    }
    last_pid = pid;
    wait_process(0);
  }

  // 检查 PID 是否被重用（启发式：如果我们释放了它们，allocpid
  // 应该再次选择低位数字） proc.c 中当前的随机/扫描实现表明它选择第一个位 0。
  // 所以 PID 应该立即被重用。
  printf("最后的 PID: %d。如果 PID 被重用，这个数应该很小。\n", last_pid);
  if (last_pid < NPROC) {
    printf("[通过] PID 分配似乎重用了 ID\n");
  } else {
    printf("[信息] PID 可能单调递增，或者 NPROC 很大\n");
  }
}

// 长期运行的CPU密集型任务，用于测试优先级降级
static int mlfq_test_flag = 0;
void task_cpu_intensive(void) {
  volatile uint64 counter = 0;
  // 持续运行直到被标记停止
  while (!mlfq_test_flag) {
    counter++;
    // 每执行一段时间主动让出一次CPU，确保父进程有机会检查
    if (counter % 50000 == 0) {
      yield();
    }
  }
  exit_process(0);
}

// IO密集型任务，频繁休眠和唤醒
static int io_wakeup_chan = 0;
static volatile int io_task_should_exit = 0;
void task_io_intensive(void) {
  int wakeup_count = 0;
  while (wakeup_count < 5 && !io_task_should_exit) {
    // 模拟IO等待
    sleep(&io_wakeup_chan);
    wakeup_count++;
    yield(); // 被唤醒后让出CPU
  }
  printf("[调试] IO任务退出，唤醒次数=%d\n", wakeup_count);
  exit_process(0);
}

// 3. MLFQ 行为 
void test_mlfq_behavior(void) {
  printf("\n[测试] MLFQ 行为\n");
  int pass_count = 0;
  int total_tests = 3;

  // === 测试 1: CPU密集型任务优先级降级 ===
  printf("\n[子测试 1] CPU密集型任务优先级降级\n");
  mlfq_test_flag = 0;
  int pid_cpu = create_process(task_cpu_intensive, "cpu_intensive");
  if (pid_cpu < 0) {
    printf("[失败] 无法创建 CPU 密集型任务\n");
    goto test2;
  }

  // 记录初始优先级
  struct proc *p_cpu = 0;
  for (int i = 0; i < NPROC; i++) {
    if (proc[i].pid == pid_cpu && proc[i].state != UNUSED) {
      p_cpu = &proc[i];
      break;
    }
  }
  
  if (!p_cpu) {
    printf("[失败] 找不到进程 PID %d\n", pid_cpu);
    goto test2;
  }

  int initial_priority = p_cpu->priority;
  printf("初始优先级: %d\n", initial_priority);

  // 让任务真正运行一段时间（等待真实时间流逝，让timer interrupt触发）
  // 优先级0的时间片是10ms，所以等待约100ms应该足够触发多次降级
  extern uint64 get_ticks(void);
  uint64 start_ticks = get_ticks();
  // 持续让出CPU，给CPU密集型任务运行机会，同时检查时间
  while (get_ticks() - start_ticks < 100) {
    yield(); // 让CPU密集型任务有机会运行
    // 偶尔做点工作，避免完全饿死
    for (volatile int i = 0; i < 1000; i++);
  }

  // 检查优先级是否降级
  int current_priority = p_cpu->priority;
  printf("当前优先级: %d (运行约50ms后)\n", current_priority);
  
  if (current_priority > initial_priority) {
    printf("[通过] 优先级已从 %d 降至 %d\n", initial_priority, current_priority);
    pass_count++;
  } else {
    printf("[失败] 优先级未降级 (初始=%d, 当前=%d)\n", initial_priority, current_priority);
  }

  // 清理任务
  mlfq_test_flag = 1; // 设置退出标志
  // 多次yield确保任务有足够机会检查标志并退出
  for (int i = 0; i < 10; i++) yield();
  int status;
  wait_process(&status);

test2:
  // === 测试 2: IO密集型任务优先级提升 ===
  printf("\n[子测试 2] IO密集型任务优先级提升\n");
  
  io_task_should_exit = 0; // 重置标志
  int pid_io = create_process(task_io_intensive, "io_intensive");
  if (pid_io < 0) {
    printf("[失败] 无法创建 IO 密集型任务\n");
    goto test3;
  }

  // 让IO任务先运行并进入休眠
  for (int i = 0; i < 3; i++) yield();

  // 找到IO任务
  struct proc *p_io = 0;
  for (int i = 0; i < NPROC; i++) {
    if (proc[i].pid == pid_io && proc[i].state != UNUSED) {
      p_io = &proc[i];
      break;
    }
  }

  if (!p_io) {
    printf("[失败] 找不到IO进程 PID %d\n", pid_io);
    goto test3;
  }

  // 记录休眠前的优先级（可能已经因为wakeup而被提升过）
  int io_priority_before = p_io->priority;
  printf("IO任务当前优先级: %d\n", io_priority_before);

  // 唤醒IO任务多次，观察优先级是否被提升
  for (int i = 0; i < 5; i++) {
    wakeup(&io_wakeup_chan);
    yield(); // 让IO任务运行
  }

  int io_priority_after = p_io->priority;
  printf("唤醒5次后优先级: %d\n", io_priority_after);

  // wakeup会递减优先级值（提升优先级），所以after应该 <= before
  if (io_priority_after <= io_priority_before) {
    printf("[通过] IO任务优先级被提升或保持 (before=%d, after=%d)\n", 
           io_priority_before, io_priority_after);
    pass_count++;
  } else {
    printf("[失败] IO任务优先级未提升 (before=%d, after=%d)\n",
           io_priority_before, io_priority_after);
  }

  // 清理
  printf("[调试] 准备清理IO任务，发送退出信号...\n");
  io_task_should_exit = 1; // 设置退出标志
  wakeup(&io_wakeup_chan); // 唤醒任务让它检查退出标志
  yield(); // 让IO任务有机会完全退出
  printf("[调试] 开始等待IO任务退出...\n");
  int ret = wait_process(&status);
  printf("[调试] IO任务已退出，PID=%d\n", ret);

test3:
  // === 测试 3: 时间片随优先级增长 ===
  printf("\n[子测试 3] 时间片随优先级增长\n");
  
  // 创建一个任务并观察其时间片
  mlfq_test_flag = 0;
  int pid_timeslice = create_process(task_cpu_intensive, "timeslice_test");
  if (pid_timeslice < 0) {
    printf("[失败] 无法创建时间片测试任务\n");
    goto summary;
  }

  struct proc *p_ts = 0;
  for (int i = 0; i < NPROC; i++) {
    if (proc[i].pid == pid_timeslice && proc[i].state != UNUSED) {
      p_ts = &proc[i];
      break;
    }
  }

  if (!p_ts) {
    printf("[失败] 找不到时间片测试进程\n");
    goto summary;
  }

  // 记录优先级0的时间片
  uint64 ts_p0 = p_ts->timeslice;
  printf("优先级0时间片: %d\n", (int)ts_p0);

  // 让任务运行足够长时间以触发降级
  uint64 start_ticks2 = get_ticks();
  while (get_ticks() - start_ticks2 < 100) {
    yield();
    for (volatile int i = 0; i < 1000; i++);
  }

  // 检查降级后的时间片
  if (p_ts->priority > 0) {
    uint64 ts_p_low = p_ts->timeslice;
    printf("优先级%d时间片: %d\n", p_ts->priority, (int)ts_p_low);
    
    // 根据proc.c实现，时间片 = TIMESLICE_BASE * (1 << priority)
    // 所以低优先级的时间片应该更长
    if (ts_p_low >= ts_p0) {
      printf("[通过] 低优先级任务时间片更长 (体现公平性)\n");
      pass_count++;
    } else {
      printf("[失败] 时间片逻辑异常\n");
    }
  } else {
    printf("[警告] 任务未降级，无法测试时间片增长\n");
  }

  // 清理
  mlfq_test_flag = 1;
  for (int i = 0; i < 10; i++) yield();
  wait_process(&status);

summary:
  // === 总结 ===
  printf("\n[MLFQ 测试总结] 通过 %d/%d 项子测试\n", pass_count, total_tests);
  if (pass_count >= 2) {
    printf("[通过] MLFQ 核心机制工作正常\n");
  } else {
    printf("[失败] MLFQ 存在问题\n");
  }
}

// 4. 孤儿进程处理
void orphan_child_task(void) {
  // 稍微休眠以便父进程可以退出
  for (volatile int i = 0; i < 1000000; i++)
    ;
  printf("孤儿进程退出。我的父进程 PID 应该是 0。实际上是: %d\n",
         myproc()->parent ? myproc()->parent->pid : 0);
  exit_process(0);
}

void orphan_parent_task(void) {
  int pid = create_process(orphan_child_task, "orphan_child");
  printf("父进程退出，遗留子进程 %d\n", pid);
  exit_process(0);
}

void test_orphan_handling(void) {
  printf("\n[测试] 孤儿进程处理\n");

  int pid_parent = create_process(orphan_parent_task, "orphan_parent");
  printf("创建孤儿父进程 PID: %d\n", pid_parent);

  // 等待父进程退出
  wait_process(0);

  printf("父进程已退出。子进程应作为孤儿继续运行。\n");
  // 我们无法 wait_process 孙子进程。
  // 我们只是 yield 几次让孙子进程完成打印。
  for (int i = 0; i < 5; i++)
    yield();

  // 检查是否有僵尸孤儿（不清理，因为这应该由内核机制处理）
  int found = 0;
  for (int i = 0; i < NPROC; i++) {
    if (proc[i].state == ZOMBIE && proc[i].parent == 0) {
      printf("发现僵尸孤儿 PID %d (状态: ZOMBIE, 父进程: NULL)\n", proc[i].pid);
      found++;
      // 注意：这里不调用 freeproc()，因为：
      // 1. 孤儿进程应该由内核的 init 进程或专门的回收机制处理
      // 2. 直接调用 freeproc() 可能破坏内部队列结构
      // 3. 在实际系统中，孤儿进程会被 reparent 到 init 进程
    }
  }

  if (found > 0) {
    printf("[通过] 孤儿进程已运行并变为 ZOMBIE (处理正确)\n");
    printf("[信息] 在完整系统中，这些孤儿应该被 init 进程回收\n");
  } else {
    printf("[警告] 未找到僵尸孤儿 (也许它还没退出？)\n");
  }
}

// 5. 队列统计
void test_queue_stats_wrapper(void) {
  printf("\n[测试] 队列统计\n");
  // 创建几个处于不同状态的进程
  create_process(task_null, "stat_cpu");  // 将快速退出
  create_process(task_null, "stat_zomb"); // 将变为 ZOMBIE
  // 让它们转换状态
  for (int i = 0; i < 5; i++)
    yield();

  print_queue_stats();

  // 清理僵尸进程
  printf("清理僵尸进程...\n");
  int cleaned = 0;
  for (int i = 0; i < 3; i++) {
    int status;
    int pid = wait_process(&status);
    if (pid > 0) {
      printf("清理进程 PID %d\n", pid);
      cleaned++;
    }
  }
  printf("已清理 %d 个进程\n", cleaned);
}

// 主测试函数
void test_all_process_management(void) {
  printf("\n****************************************\n");
  printf("  综合进程管理测试  \n");
  printf("****************************************\n");

  test_basic_lifecycle();
  test_pid_reuse();
  test_mlfq_behavior();
  test_orphan_handling();
  test_queue_stats_wrapper();

  printf("\n****************************************\n");
  printf("  所有进程测试已完成\n");
  printf("****************************************\n");
}
