#include "proc.h"

typedef unsigned long uint64;

extern int printf(const char *fmt, ...);
extern uint64 get_ticks(void);
extern void yield(void);

static int shared_counter = 0;

void task_high_priority(void) {
    for (int i = 0; i < 3; i++) {
        printf("[High] Running iteration %d\n", i);
        for (volatile int j = 0; j < 1000000; j++);
        yield();
    }
    printf("[High] Task completed\n");
    exit_process(0);
}

void task_medium_priority(void) {
    for (int i = 0; i < 3; i++) {
        printf("[Medium] Running iteration %d\n", i);
        for (volatile int j = 0; j < 1000000; j++);
        yield();
    }
    printf("[Medium] Task completed\n");
    exit_process(0);
}

void task_low_priority(void) {
    for (int i = 0; i < 3; i++) {
        printf("[Low] Running iteration %d\n", i);
        for (volatile int j = 0; j < 1000000; j++);
        yield();
    }
    printf("[Low] Task completed\n");
    exit_process(0);
}

void producer_task(void) {
    for (int i = 0; i < 5; i++) {
        shared_counter++;
        printf("[Producer] Produced: %d\n", shared_counter);
        wakeup(&shared_counter);
        for (volatile int j = 0; j < 500000; j++);
        yield();
    }
    printf("[Producer] Task completed\n");
    exit_process(0);
}

void consumer_task(void) {
    for (int i = 0; i < 5; i++) {
        while (shared_counter == 0) {
            sleep(&shared_counter);
        }
        printf("[Consumer] Consumed: %d\n", shared_counter);
        shared_counter--;
        for (volatile int j = 0; j < 500000; j++);
        yield();
    }
    printf("[Consumer] Task completed\n");
    exit_process(0);
}

void test_process_creation(void) {
    printf("\n=== Process Creation Test ===\n");
    
    int pid = create_process(task_medium_priority, "test_proc", 2);
    if (pid > 0) {
        printf("Created process with PID %d\n", pid);
        int status;
        int ret = wait_process(&status);
        printf("Process %d exited with status %d\n", ret, status);
    } else {
        printf("Failed to create process\n");
    }
}

void test_priority_scheduling(void) {
    printf("\n=== Priority Scheduling Test ===\n");
    
    create_process(task_low_priority, "low", 3);
    create_process(task_high_priority, "high", 0);
    create_process(task_medium_priority, "medium", 2);
    
    printf("Created 3 processes with different priorities\n");
    printf("Waiting for all processes to complete...\n");
    
    for (int i = 0; i < 3; i++) {
        wait_process(0);
    }
    
    printf("All processes completed\n");
}

void test_synchronization(void) {
    printf("\n=== Synchronization Test ===\n");
    
    shared_counter = 0;
    
    create_process(producer_task, "producer", 2);
    create_process(consumer_task, "consumer", 2);
    
    printf("Created producer and consumer processes\n");
    
    wait_process(0);
    wait_process(0);
    
    printf("Synchronization test completed\n");
}

