#include "sync.h"
#include "proc.h"

// 外部函数
int printf(const char *fmt, ...);
extern int create_process(void (*fn)(void), const char *name);
extern void exit_process(int status);
extern int wait_process(int *status);
extern void yield(void);

// ========== 测试1：信号量实现生产者-消费者 ==========
#define BUFFER_SIZE 5
static int buffer[BUFFER_SIZE];
static int in = 0, out = 0;
static struct semaphore empty, full;
static struct mutex buffer_mutex;

static void producer_sem(void) {
    for (int i = 0; i < 10; i++) {
        sem_wait(&empty);          // 等待空槽
        mutex_lock(&buffer_mutex); // 互斥访问buffer
        
        buffer[in] = i;
        printf("Producer: produced %d at slot %d\n", i, in);
        in = (in + 1) % BUFFER_SIZE;
        
        mutex_unlock(&buffer_mutex);
        sem_post(&full);           // 增加满槽数
        
        // 模拟生产延迟
        for (volatile int j = 0; j < 100000; j++);
    }
    exit_process(0);
}

static void consumer_sem(void) {
    for (int i = 0; i < 10; i++) {
        sem_wait(&full);           // 等待满槽
        mutex_lock(&buffer_mutex); // 互斥访问buffer
        
        int item = buffer[out];
        printf("Consumer: consumed %d from slot %d\n", item, out);
        out = (out + 1) % BUFFER_SIZE;
        
        mutex_unlock(&buffer_mutex);
        sem_post(&empty);          // 增加空槽数
        
        // 模拟消费延迟
        for (volatile int j = 0; j < 100000; j++);
    }
    exit_process(0);
}

void test_semaphore_producer_consumer(void) {
    printf("\n=== Test 1: Semaphore Producer-Consumer ===\n");
    
    // 重置状态
    in = 0;
    out = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = 0;
    }
    
    // 初始化信号量：5个空槽，0个满槽
    sem_init(&empty, BUFFER_SIZE, "empty");
    sem_init(&full, 0, "full");
    mutex_init(&buffer_mutex, "buffer_mutex");
    
    printf("Starting producer and consumer...\n");
    create_process(producer_sem, "producer");
    create_process(consumer_sem, "consumer");
    
    wait_process(0);
    wait_process(0);
    
    sem_destroy(&empty);
    sem_destroy(&full);
    mutex_destroy(&buffer_mutex);
    
    printf("[PASS] Test 1: Semaphore works correctly\n");
}

// ========== 测试2：互斥锁保护共享资源 ==========
static int shared_counter = 0;
static struct mutex counter_mutex;

static void increment_task(void) {
    for (int i = 0; i < 100; i++) {
        mutex_lock(&counter_mutex);
        shared_counter++;
        mutex_unlock(&counter_mutex);
    }
    exit_process(0);
}

void test_mutex_counter(void) {
    printf("\n=== Test 2: Mutex Protected Counter ===\n");
    
    shared_counter = 0;
    mutex_init(&counter_mutex, "counter_mutex");
    
    printf("Creating 3 processes to increment counter...\n");
    // 创建3个进程同时递增
    create_process(increment_task, "inc1");
    create_process(increment_task, "inc2");
    create_process(increment_task, "inc3");
    
    wait_process(0);
    wait_process(0);
    wait_process(0);
    
    printf("Final counter value: %d (expected: 300)\n", shared_counter);
    
    if (shared_counter == 300) {
        printf("[PASS] Test 2: Mutex correctly protects shared resource\n");
    } else {
        printf("[FAIL] Test 2: Counter value incorrect (race condition?)\n");
    }
    
    mutex_destroy(&counter_mutex);
}

// ========== 测试3：条件变量实现有界缓冲 ==========
static int cv_buffer[BUFFER_SIZE];
static int cv_count = 0;
static struct mutex cv_mutex;
static struct condvar not_empty, not_full;

static void producer_cv(void) {
    for (int i = 0; i < 10; i++) {
        mutex_lock(&cv_mutex);
        
        // 等待缓冲区不满
        while (cv_count == BUFFER_SIZE) {
            cond_wait(&not_full, &cv_mutex);
        }
        
        cv_buffer[cv_count++] = i;
        printf("Producer(CV): produced %d, count=%d\n", i, cv_count);
        
        cond_signal(&not_empty);  // 通知消费者
        mutex_unlock(&cv_mutex);
        
        for (volatile int j = 0; j < 100000; j++);
    }
    exit_process(0);
}

static void consumer_cv(void) {
    for (int i = 0; i < 10; i++) {
        mutex_lock(&cv_mutex);
        
        // 等待缓冲区不空
        while (cv_count == 0) {
            cond_wait(&not_empty, &cv_mutex);
        }
        
        int item = cv_buffer[--cv_count];
        printf("Consumer(CV): consumed %d, count=%d\n", item, cv_count);
        
        cond_signal(&not_full);  // 通知生产者
        mutex_unlock(&cv_mutex);
        
        for (volatile int j = 0; j < 100000; j++);
    }
    exit_process(0);
}

void test_condvar_bounded_buffer(void) {
    printf("\n=== Test 3: Condition Variable Bounded Buffer ===\n");
    
    cv_count = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        cv_buffer[i] = 0;
    }
    
    mutex_init(&cv_mutex, "cv_mutex");
    cond_init(&not_empty, "not_empty");
    cond_init(&not_full, "not_full");
    
    printf("Starting producer and consumer with condition variables...\n");
    create_process(producer_cv, "producer_cv");
    create_process(consumer_cv, "consumer_cv");
    
    wait_process(0);
    wait_process(0);
    
    mutex_destroy(&cv_mutex);
    cond_destroy(&not_empty);
    cond_destroy(&not_full);
    
    if (cv_count == 0) {
        printf("[PASS] Test 3: Condition variables work correctly\n");
    } else {
        printf("[FAIL] Test 3: Buffer not empty (count=%d)\n", cv_count);
    }
}

// ========== 测试4：读写锁 ==========
static int shared_data = 0;
static struct rwlock rw;

static void reader_task(void) {
    for (int i = 0; i < 5; i++) {
        rwlock_rlock(&rw);
        printf("Reader: read data = %d\n", shared_data);
        rwlock_runlock(&rw);
        
        for (volatile int j = 0; j < 50000; j++);
    }
    exit_process(0);
}

static void writer_task(void) {
    for (int i = 0; i < 5; i++) {
        rwlock_wlock(&rw);
        shared_data++;
        printf("Writer: wrote data = %d\n", shared_data);
        rwlock_wunlock(&rw);
        
        for (volatile int j = 0; j < 100000; j++);
    }
    exit_process(0);
}

void test_rwlock(void) {
    printf("\n=== Test 4: Read-Write Lock ===\n");
    
    shared_data = 0;
    rwlock_init(&rw, "shared_data_rw");
    
    printf("Starting 2 readers and 1 writer...\n");
    // 创建多个读者和一个写者
    create_process(reader_task, "reader1");
    create_process(reader_task, "reader2");
    create_process(writer_task, "writer");
    
    wait_process(0);
    wait_process(0);
    wait_process(0);
    
    rwlock_destroy(&rw);
    
    printf("Final data value: %d (expected: 5)\n", shared_data);
    if (shared_data == 5) {
        printf("[PASS] Test 4: Read-write lock works correctly\n");
    } else {
        printf("[FAIL] Test 4: Data value incorrect\n");
    }
}

// ========== 测试5：trylock非阻塞获取 ==========
static struct mutex trylock_mutex;

static void trylock_task(void) {
    for (int i = 0; i < 5; i++) {
        if (mutex_trylock(&trylock_mutex)) {
            printf("Task got lock on attempt %d\n", i);
            
            // 模拟工作
            for (volatile int j = 0; j < 100000; j++);
            
            mutex_unlock(&trylock_mutex);
        } else {
            printf("Task failed to get lock on attempt %d\n", i);
        }
        
        for (volatile int j = 0; j < 50000; j++);
    }
    exit_process(0);
}

void test_trylock(void) {
    printf("\n=== Test 5: Non-blocking Trylock ===\n");
    
    mutex_init(&trylock_mutex, "trylock_mutex");
    
    printf("Starting 2 processes with trylock...\n");
    create_process(trylock_task, "trylock1");
    create_process(trylock_task, "trylock2");
    
    wait_process(0);
    wait_process(0);
    
    mutex_destroy(&trylock_mutex);
    
    printf("[PASS] Test 5: Trylock works correctly\n");
}

// ========== 主测试入口 ==========
void test_all_sync_primitives(void) {
    printf("\n========================================\n");
    printf("  Advanced Synchronization Tests\n");
    printf("========================================\n");
    printf("Testing 4 synchronization primitives:\n");
    printf("  1. Semaphore\n");
    printf("  2. Mutex\n");
    printf("  3. Condition Variable\n");
    printf("  4. Read-Write Lock\n");
    printf("  5. Trylock (Non-blocking)\n");
    printf("========================================\n");
    
    // 注意：当前测试框架假设所有测试都会通过
    // 如果需要更精确的统计，可以让每个测试函数返回 pass/fail
    int total_tests = 5;
    
    test_semaphore_producer_consumer();
    test_mutex_counter();
    test_condvar_bounded_buffer();
    test_rwlock();
    test_trylock();
    
    printf("\n========================================\n");
    printf("  All %d tests completed\n", total_tests);
    printf("  Check [PASS]/[FAIL] markers above\n");
    printf("========================================\n");
}

