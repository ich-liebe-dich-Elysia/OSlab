#include "riscv.h"

typedef unsigned long uint64;
typedef unsigned int uint32;

// 函数声明
void printf(const char *fmt, ...);
void consputc(int c);
void uart_intr(void);
void handle_exception(uint64 cause, uint64 sepc, uint64 stval);

// PLIC寄存器地址
#define PLIC_BASE 0x0c000000L
#define PLIC_PRIORITY (PLIC_BASE + 0x0)
#define PLIC_PENDING (PLIC_BASE + 0x1000)
#define PLIC_SENABLE (PLIC_BASE + 0x2080)
#define PLIC_SCLAIM (PLIC_BASE + 0x201004)

#define UART0_IRQ 10

// 中断向量
extern void kernelvec();

// 中断处理函数类型
typedef void (*interrupt_handler_t)(void);

// 中断处理函数表
#define MAX_IRQ 32
static interrupt_handler_t interrupt_handlers[MAX_IRQ];

// 系统时钟计数
static volatile uint64 ticks = 0;

/*
 * 初始化中断系统
 */
void trap_init(void) {
    // 清空中断处理函数表
    for (int i = 0; i < MAX_IRQ; i++) {
        interrupt_handlers[i] = 0;
    }
}

/*
 * 设置中断向量表
 */
void trap_inithart(void) {
    w_stvec((uint64)kernelvec);
}

/*
 * 注册中断处理函数
 */
void register_interrupt(int irq, interrupt_handler_t handler) {
    if (irq >= 0 && irq < MAX_IRQ) {
        interrupt_handlers[irq] = handler;
    }
}

/*
 * 开启特定中断
 */
void enable_interrupt(int irq) {
    if (irq == 5) {  // 时钟中断
        w_sie(r_sie() | SIE_STIE);
    }
}

/*
 * 关闭特定中断
 */
void disable_interrupt(int irq) {
    if (irq == 5) {  // 时钟中断
        w_sie(r_sie() & ~SIE_STIE);
    }
}

/*
 * 时钟中断处理函数
 */
void timer_interrupt(void) {
    ticks++;
    
    // 设置下次中断时间（1ms）
    w_stimecmp(r_time() + 1000000);
}

/*
 * 获取系统时钟
 */
uint64 get_ticks(void) {
    return ticks;
}

/*
 * PLIC初始化
 */
void plic_init(void) {
    // 设置UART中断优先级
    *(uint32*)(PLIC_PRIORITY + UART0_IRQ * 4) = 1;
}

/*
 * PLIC hart初始化
 */
void plic_inithart(void) {
    // 使能UART中断
    *(uint32*)PLIC_SENABLE = (1 << UART0_IRQ);
}

/*
 * 获取中断号
 */
static int plic_claim(void) {
    return *(uint32*)PLIC_SCLAIM;
}

/*
 * 完成中断处理
 */
static void plic_complete(int irq) {
    *(uint32*)PLIC_SCLAIM = irq;
}

/*
 * 设备中断处理
 * 返回值：2=时钟中断，1=其他设备，0=未识别
 */
static int devintr(void) {
    uint64 scause = r_scause();
    
    if (scause == 0x8000000000000005L) {
        // 时钟中断
        timer_interrupt();
        return 2;
    } else if (scause == 0x8000000000000009L) {
        // 外部中断（PLIC）
        int irq = plic_claim();
        
        if (irq == UART0_IRQ) {
            uart_intr();
        } else if (irq) {
            printf("Unexpected interrupt irq=%d\n", irq);
        }
        
        if (irq) {
            plic_complete(irq);
        }
        
        return 1;
    }
    
    return 0;
}

/*
 * 内核态中断处理
 */
void kerneltrap(void) {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();
    uint64 stval = r_stval();
    
    // 检查是否从S模式进入
    if ((sstatus & SSTATUS_SPP) == 0) {
        printf("kerneltrap: not from supervisor mode\n");
        while(1);
    }
    
    // 检查是否是中断（最高位为1）
    if (scause & (1UL << 63)) {
        // 这是中断
        int which_dev = devintr();
        
        if (which_dev == 0) {
            printf("Unknown interrupt: scause=0x%lx\n", scause);
            while(1);
        }
    } else {
        // 这是异常
        handle_exception(scause, sepc, stval);
    }
    
    // 恢复trap寄存器
    w_sepc(sepc);
    w_sstatus(sstatus);
}

/*
 * 页错误处理
 */
static void handle_page_fault(uint64 cause, uint64 sepc, uint64 stval) {
    const char *fault_type;
    
    switch (cause) {
        case 12:
            fault_type = "Instruction";
            break;
        case 13:
            fault_type = "Load";
            break;
        case 15:
            fault_type = "Store";
            break;
        default:
            fault_type = "Unknown";
            break;
    }
    
    printf("\n=== Page Fault ===\n");
    printf("Type: %s page fault\n", fault_type);
    printf("Faulting address: 0x%lx\n", stval);
    printf("Instruction address: 0x%lx\n", sepc);
    printf("Cause: Access to unmapped or protected memory\n");
    
    // 简单的诊断信息
    if (stval == 0) {
        printf("Diagnosis: NULL pointer dereference\n");
    } else if (stval < 0x1000) {
        printf("Diagnosis: Low address access (likely NULL+offset)\n");
    } else if (stval >= 0x80000000 && stval < 0x88000000) {
        printf("Diagnosis: Kernel address space access\n");
    } else {
        printf("Diagnosis: Invalid address access\n");
    }
    
    printf("System halted.\n");
    while(1);
}

/*
 * 非法指令处理
 */
static void handle_illegal_instruction(uint64 sepc, uint64 stval) {
    printf("\n=== Illegal Instruction ===\n");
    printf("Instruction address: 0x%lx\n", sepc);
    printf("Instruction value: 0x%lx\n", stval);
    printf("Cause: Attempted to execute invalid or privileged instruction\n");
    printf("System halted.\n");
    while(1);
}

/*
 * 断点异常处理
 */
static void handle_breakpoint(uint64 sepc) {
    printf("\n=== Breakpoint ===\n");
    printf("Breakpoint at: 0x%lx\n", sepc);
    printf("Continuing execution...\n");
    // 断点不停机，返回继续执行
}

/*
 * 环境调用（系统调用）处理
 */
static void handle_ecall(uint64 sepc) {
    printf("\n=== System Call ===\n");
    printf("Ecall from: 0x%lx\n", sepc);
    printf("Note: System call interface not implemented\n");
    printf("System halted.\n");
    while(1);
}

/*
 * 异常处理总入口
 */
void handle_exception(uint64 cause, uint64 sepc, uint64 stval) {
    switch (cause) {
        case 2:  // 非法指令
            handle_illegal_instruction(sepc, stval);
            break;
            
        case 3:  // 断点
            handle_breakpoint(sepc);
            break;
            
        case 8:  // 来自U模式的环境调用
        case 9:  // 来自S模式的环境调用
            handle_ecall(sepc);
            break;
            
        case 12: // 指令页故障
        case 13: // 加载页故障
        case 15: // 存储页故障
            handle_page_fault(cause, sepc, stval);
            break;
            
        case 1:  // 指令访问故障
            printf("\n=== Instruction Access Fault ===\n");
            printf("Address: 0x%lx\n", sepc);
            printf("System halted.\n");
            while(1);
            break;
            
        case 5:  // 加载访问故障
            printf("\n=== Load Access Fault ===\n");
            printf("Address: 0x%lx, Value: 0x%lx\n", sepc, stval);
            printf("System halted.\n");
            while(1);
            break;
            
        case 7:  // 存储访问故障
            printf("\n=== Store Access Fault ===\n");
            printf("Address: 0x%lx, Value: 0x%lx\n", sepc, stval);
            printf("System halted.\n");
            while(1);
            break;
            
        default:
            printf("\n=== Unknown Exception ===\n");
            printf("Cause: 0x%lx\n", cause);
            printf("PC: 0x%lx\n", sepc);
            printf("Value: 0x%lx\n", stval);
            printf("System halted.\n");
            while(1);
            break;
    }
}

