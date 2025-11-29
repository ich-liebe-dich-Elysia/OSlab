#include "proc.h"
#include "riscv.h"

typedef unsigned long uint64;

struct cpu cpus[1];
struct proc proc[NPROC];

static uint64 pid_bitmap[(NPROC + 63) / 64];
static struct proc *runqueue[NPRIO];

extern void printf(const char *fmt, ...);
extern void* alloc_page(void);
extern void free_page(void *pa);
extern pagetable_t create_pagetable(void);
extern void free_pagetable(pagetable_t pagetable);
extern void swtch(struct context *old, struct context *new);
extern uint64 r_tp(void);

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

static void freepid(int pid) {
    if (pid <= 0 || pid > NPROC) return;
    int i = pid - 1;
    int word = i / 64;
    int bit = i % 64;
    pid_bitmap[word] &= ~(1UL << bit);
}

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
}

int cpuid(void) {
    return 0;
}

struct cpu* mycpu(void) {
    return &cpus[0];
}

struct proc* myproc(void) {
    struct cpu *c = mycpu();
    return c->proc;
}

static void push_off(void) {
    int old = intr_get();
    intr_off();
    struct cpu *c = mycpu();
    if (c->noff == 0)
        c->intena = old;
    c->noff += 1;
}

static void pop_off(void) {
    struct cpu *c = mycpu();
    if (intr_get())
        while(1);
    if (c->noff < 1)
        while(1);
    c->noff -= 1;
    if (c->noff == 0 && c->intena)
        intr_on();
}

struct proc* allocproc(void) {
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
    p->priority = 2;
    p->runtime = 0;
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
    
    p->pagetable = create_pagetable();
    if (p->pagetable == 0) {
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
    if (p->kstack) {
        free_page((void*)(p->kstack - 4096));
        p->kstack = 0;
    }
    if (p->pagetable) {
        free_pagetable(p->pagetable);
        p->pagetable = 0;
    }
    freepid(p->pid);
    p->pid = 0;
    p->state = UNUSED;
    p->chan = 0;
    p->xstate = 0;
    p->parent = 0;
    p->next = 0;
}

static void enqueue(struct proc *p) {
    int prio = p->priority;
    if (prio < 0) prio = 0;
    if (prio >= NPRIO) prio = NPRIO - 1;
    
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

static struct proc* dequeue(void) {
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

int create_process(void (*entry)(void), const char *name, int priority) {
    struct proc *p = allocproc();
    if (p == 0)
        return -1;
    
    if (name) {
        for (int i = 0; i < 15 && name[i]; i++)
            p->name[i] = name[i];
    }
    
    p->priority = priority;
    
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
    
    if (p->parent)
        wakeup(p->parent);
    
    sched();
}

int wait_process(int *status) {
    struct proc *p = myproc();
    if (p == 0)
        return -1;
    
    for (;;) {
        int havekids = 0;
        for (int i = 0; i < NPROC; i++) {
            struct proc *child = &proc[i];
            if (child->parent == p) {
                havekids = 1;
                if (child->state == ZOMBIE) {
                    int pid = child->pid;
                    if (status)
                        *status = child->xstate;
                    freeproc(child);
                    return pid;
                }
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
        while(1);
    
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
    sched();
}

void wakeup(void *chan) {
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc[i];
        if (p->state == SLEEPING && p->chan == chan) {
            p->chan = 0;
            p->state = RUNNABLE;
            enqueue(p);
        }
    }
}

