#ifndef PROC_H
#define PROC_H

#include "riscv.h"

#define NPROC 64
#define NPRIO 4

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

struct cpu {
    struct proc *proc;
    struct context context;
    int noff;
    int intena;
};

extern struct cpu cpus[1];

enum procstate { UNUSED, USED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };

struct proc {
    enum procstate state;
    int pid;
    int priority;
    uint64 runtime;
    uint64 kstack;
    pagetable_t pagetable;
    struct context context;
    void *chan;
    int xstate;
    struct proc *parent;
    struct proc *next;
    char name[16];
};

extern struct proc proc[NPROC];

void procinit(void);
struct proc* allocproc(void);
void freeproc(struct proc *p);
int create_process(void (*entry)(void), const char *name, int priority);
void exit_process(int status);
int wait_process(int *status);
void scheduler(void);
void sched(void);
void yield(void);
void sleep(void *chan);
void wakeup(void *chan);
struct proc* myproc(void);
struct cpu* mycpu(void);
int cpuid(void);

#endif

