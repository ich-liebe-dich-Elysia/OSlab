#ifndef SYNC_H
#define SYNC_H

// ========== 信号量 ==========
struct semaphore {
    int value;              // 信号量值
    void *waitqueue;        // 等待队列（使用地址作为通道）
    char name[16];          // 调试用名称
};

void sem_init(struct semaphore *sem, int value, const char *name);
void sem_wait(struct semaphore *sem);    // P操作/down
int sem_trywait(struct semaphore *sem);  // 非阻塞P操作
void sem_post(struct semaphore *sem);    // V操作/up
void sem_destroy(struct semaphore *sem);

// ========== 互斥锁 ==========
struct mutex {
    int locked;             // 0=未锁定, 1=已锁定
    void *owner;            // 持有锁的进程
    void *waitqueue;        // 等待队列
    char name[16];          // 调试用名称
};

void mutex_init(struct mutex *m, const char *name);
void mutex_lock(struct mutex *m);
int mutex_trylock(struct mutex *m);      // 非阻塞获锁
void mutex_unlock(struct mutex *m);
void mutex_destroy(struct mutex *m);
int mutex_holding(struct mutex *m);      // 检查是否持有锁

// ========== 条件变量 ==========
struct condvar {
    void *waitqueue;        // 等待队列
    char name[16];          // 调试用名称
};

void cond_init(struct condvar *cv, const char *name);
void cond_wait(struct condvar *cv, struct mutex *m);  // 原子释放锁并等待
void cond_signal(struct condvar *cv);                 // 唤醒一个等待进程
void cond_broadcast(struct condvar *cv);              // 唤醒所有等待进程
void cond_destroy(struct condvar *cv);

// ========== 读写锁 ==========
struct rwlock {
    int readers;            // 当前读者数量
    int writer;             // 是否有写者（0或1）
    struct semaphore read_sem;   // 读信号量
    struct semaphore write_sem;  // 写信号量
    struct mutex lock;           // 保护内部状态
    char name[16];
};

void rwlock_init(struct rwlock *rw, const char *name);
void rwlock_rlock(struct rwlock *rw);    // 获取读锁
void rwlock_runlock(struct rwlock *rw);  // 释放读锁
void rwlock_wlock(struct rwlock *rw);    // 获取写锁
void rwlock_wunlock(struct rwlock *rw);  // 释放写锁
void rwlock_destroy(struct rwlock *rw);

#endif
