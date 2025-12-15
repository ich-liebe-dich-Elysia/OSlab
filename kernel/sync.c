#include "sync.h"
#include "proc.h"
#include "riscv.h"

// 外部函数声明
int printf(const char *fmt, ...);
extern void sleep(void *chan);
extern void wakeup(void *chan);
extern struct proc *myproc(void);

// 字符串复制辅助函数
static void safestrcpy(char *s, const char *t, int n) {

  if (n <= 0)
    return;
  while (--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
}

// ========== 信号量实现 ==========

void sem_init(struct semaphore *sem, int value, const char *name) {
  sem->value = value;
  sem->waitqueue = sem; // 使用自身地址作为等待通道
  safestrcpy(sem->name, name, sizeof(sem->name));
}

void sem_wait(struct semaphore *sem) {
  int old = intr_get();
  intr_off();

  // P操作：value减1
  sem->value--;

  // 如果value < 0，说明资源不足，需要阻塞
  while (sem->value < 0) {
    sleep(sem->waitqueue);
  }

  if (old)
    intr_on();
}

int sem_trywait(struct semaphore *sem) {
  int old = intr_get();
  intr_off();

  int ret = 0;
  if (sem->value > 0) {
    sem->value--;
    ret = 1;
  }

  if (old)
    intr_on();
  return ret;
}

void sem_post(struct semaphore *sem) {
  int old = intr_get();
  intr_off();

  // V操作：value加1
  sem->value++;

  // 如果有进程在等待（value <= 0之前），唤醒一个
  if (sem->value <= 0) {
    wakeup(sem->waitqueue);
  }

  if (old)
    intr_on();
}

void sem_destroy(struct semaphore *sem) {
  sem->value = 0;
  sem->waitqueue = 0;
}

// ========== 互斥锁实现 ==========

void mutex_init(struct mutex *m, const char *name) {
  m->locked = 0;
  m->owner = 0;
  m->waitqueue = m;
  safestrcpy(m->name, name, sizeof(m->name));
}

void mutex_lock(struct mutex *m) {
  int old = intr_get();
  intr_off();

  // 如果已经被锁定，进入等待
  while (m->locked) {
    sleep(m->waitqueue);
  }

  // 获取锁
  m->locked = 1;
  m->owner = myproc();

  if (old)
    intr_on();
}

int mutex_trylock(struct mutex *m) {
  int old = intr_get();
  intr_off();

  int ret = 0;
  if (!m->locked) {
    m->locked = 1;
    m->owner = myproc();
    ret = 1;
  }

  if (old)
    intr_on();
  return ret;
}

void mutex_unlock(struct mutex *m) {
  int old = intr_get();
  intr_off();

  // 检查是否持有锁
  if (m->owner != myproc()) {
    printf("mutex_unlock: not owner\n");
    if (old)
      intr_on();
    return;
  }

  // 释放锁
  m->locked = 0;
  m->owner = 0;

  // 唤醒一个等待进程
  wakeup(m->waitqueue);

  if (old)
    intr_on();
}

void mutex_destroy(struct mutex *m) {
  m->locked = 0;
  m->owner = 0;
  m->waitqueue = 0;
}

int mutex_holding(struct mutex *m) { return m->locked && m->owner == myproc(); }

// ========== 条件变量实现 ==========

void cond_init(struct condvar *cv, const char *name) {
  cv->waitqueue = cv;
  safestrcpy(cv->name, name, sizeof(cv->name));
}

void cond_wait(struct condvar *cv, struct mutex *m) {
  // 检查是否持有互斥锁
  if (!mutex_holding(m)) {
    printf("cond_wait: must hold mutex\n");
    return;
  }

  int old = intr_get();
  intr_off();

  // 原子地释放互斥锁并进入睡眠
  m->locked = 0;
  m->owner = 0;
  wakeup(m->waitqueue); // 唤醒等待mutex的进程

  sleep(cv->waitqueue);

  // 被唤醒后重新获取互斥锁
  while (m->locked) {
    sleep(m->waitqueue);
  }
  m->locked = 1;
  m->owner = myproc();

  if (old)
    intr_on();
}

void cond_signal(struct condvar *cv) {
  int old = intr_get();
  intr_off();

  wakeup(cv->waitqueue); // 唤醒一个等待进程

  if (old)
    intr_on();
}

void cond_broadcast(struct condvar *cv) {
  int old = intr_get();
  intr_off();

  wakeup(cv->waitqueue); // 唤醒所有等待进程

  if (old)
    intr_on();
}

void cond_destroy(struct condvar *cv) { cv->waitqueue = 0; }

// ========== 读写锁实现 ==========

void rwlock_init(struct rwlock *rw, const char *name) {
  rw->readers = 0;
  rw->writer = 0;
  sem_init(&rw->read_sem, 1, "read_sem");
  sem_init(&rw->write_sem, 1, "write_sem");
  mutex_init(&rw->lock, "rwlock_mutex");
  safestrcpy(rw->name, name, sizeof(rw->name));
}

void rwlock_rlock(struct rwlock *rw) {
  mutex_lock(&rw->lock);

  rw->readers++;
  if (rw->readers == 1) {
    // 第一个读者需要获取写锁，阻止写者
    sem_wait(&rw->write_sem);
  }

  mutex_unlock(&rw->lock);
}

void rwlock_runlock(struct rwlock *rw) {
  mutex_lock(&rw->lock);

  rw->readers--;
  if (rw->readers == 0) {
    // 最后一个读者释放写锁，允许写者进入
    sem_post(&rw->write_sem);
  }

  mutex_unlock(&rw->lock);
}

void rwlock_wlock(struct rwlock *rw) {
  // 写者需要获取写锁
  sem_wait(&rw->write_sem);
  rw->writer = 1;
}

void rwlock_wunlock(struct rwlock *rw) {
  rw->writer = 0;
  sem_post(&rw->write_sem);
}

void rwlock_destroy(struct rwlock *rw) {
  sem_destroy(&rw->read_sem);
  sem_destroy(&rw->write_sem);
  mutex_destroy(&rw->lock);
}
