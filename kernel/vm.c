/*
 * 虚拟内存管理
 */

typedef unsigned long uint64;
typedef uint64 pte_t;
typedef uint64 *pagetable_t;

// 函数声明
void *alloc_page(void);
void free_page(void *page);
int printf(const char *fmt, ...);
void *memset(void *dst, int c, uint64 n);

// 页表和内存常量
#define PGSIZE 4096
#define PGSHIFT 12
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

// 物理内存布局
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)
#define UART0 0x10000000L
#define VIRTIO0 0x10001000L
#define PLIC 0x0c000000L

// 页表项标志位
#define PTE_V (1L << 0)  // 有效位
#define PTE_R (1L << 1)  // 可读
#define PTE_W (1L << 2)  // 可写
#define PTE_X (1L << 3)  // 可执行
#define PTE_U (1L << 4)  // 用户可访问

// 地址操作宏
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

// PTE操作宏
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 虚拟地址解析
#define PXMASK 0x1FF // 9位
#define PXSHIFT(level) (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)

// 外部符号
extern char etext[];
extern char trampoline[];

// 全局内核页表
pagetable_t kernel_pagetable;

/*
 * 页表遍历 - 找到虚拟地址对应的PTE
 * alloc: 如果中间级页表不存在，是否分配
 */
static pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
    if (va >= MAXVA)
        return 0;
    
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            if (!alloc)
                return 0;
            
            pagetable = (pagetable_t)alloc_page();
            if (pagetable == 0)
                return 0;
            
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    
    return &pagetable[PX(0, va)];
}

/*
 * 创建虚拟地址到物理地址的映射
 */
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) {
    uint64 a, last;
    pte_t *pte;
    
    if (size == 0)
        return 0;
    
    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    
    for (;;) {
        if ((pte = walk(pagetable, a, 1)) == 0)
            return -1;
        
        if (*pte & PTE_V)
            return -1;  // 已经映射
        
        *pte = PA2PTE(pa) | perm | PTE_V;
        
        if (a == last)
            break;
        
        a += PGSIZE;
        pa += PGSIZE;
    }
    
    return 0;
}

/*
 * 创建空页表
 */
pagetable_t create_pagetable(void) {
    pagetable_t pt = (pagetable_t)alloc_page();
    if (pt)
        memset(pt, 0, PGSIZE);
    return pt;
}

/*
 * 递归释放页表
 */
void freewalk(pagetable_t pagetable, int level) {
    if (level > 0) {
        // 递归释放下级页表
        for (int i = 0; i < 512; i++) {
            pte_t pte = pagetable[i];
            if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
                uint64 child = PTE2PA(pte);
                freewalk((pagetable_t)child, level - 1);
                pagetable[i] = 0;
            }
        }
    }
    
    free_page((void*)pagetable);
}

/*
 * 销毁页表
 */
void destroy_pagetable(pagetable_t pagetable) {
    freewalk(pagetable, 2);
}

/*
 * 将一段内存区域映射到内核页表
 */
static void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    if (mappages(kpgtbl, va, sz, pa, perm) != 0) {
        printf("kvmmap failed\n");
    }
}

/*
 * 创建内核页表 - 建立恒等映射
 */
static pagetable_t kvmmake(void) {
    pagetable_t kpgtbl = create_pagetable();
    if (kpgtbl == 0)
        return 0;
    
    // UART寄存器
    kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
    
    // virtio磁盘接口
    kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
    
    // PLIC中断控制器
    kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
    
    // 内核代码段 - 可读可执行
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);
    
    // 内核数据段和剩余物理内存 - 可读可写
    // 从etext向上对齐到页边界，避免与代码段最后一页重叠
    uint64 data_start = PGROUNDUP((uint64)etext);
    kvmmap(kpgtbl, data_start, data_start, PHYSTOP - data_start, PTE_R | PTE_W);
    
    return kpgtbl;
}

/*
 * 初始化内核页表
 */
void kvminit(void) {
    kernel_pagetable = kvmmake();
    if (kernel_pagetable == 0) {
        printf("kvminit failed\n");
    }
}

/*
 * 启用分页 - 设置SATP寄存器
 */
void kvminithart(void) {
    // 等待之前的内存写入完成
    asm volatile("sfence.vma zero, zero");
    
    // 设置SATP寄存器
    // MODE=8 (Sv39), ASID=0, PPN=(物理页号)
    uint64 satp = (8L << 60) | ((uint64)kernel_pagetable >> 12);
    asm volatile("csrw satp, %0" : : "r" (satp));
    
    // 刷新TLB
    asm volatile("sfence.vma zero, zero");
}

/*
 * 虚拟地址转物理地址
 */
uint64 walkaddr(pagetable_t pagetable, uint64 va) {
    pte_t *pte = walk(pagetable, va, 0);
    
    if (pte == 0)
        return 0;
    if ((*pte & PTE_V) == 0)
        return 0;
    
    return PTE2PA(*pte);
}

/*
 * 打印页表内容
 */
void dump_pagetable(pagetable_t pagetable, int level, uint64 va_base) {
    static const char *indent[] = {"", "  ", "    ", "      "};
    
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if (pte & PTE_V) {
            uint64 va = va_base + (i << PXSHIFT(level));
            printf("%s[%d] pte=%p pa=%p", indent[2-level], i, pte, PTE2PA(pte));
            
            if (pte & PTE_R) printf(" R");
            if (pte & PTE_W) printf(" W");
            if (pte & PTE_X) printf(" X");
            if (pte & PTE_U) printf(" U");
            printf("\n");
            
            if (level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
                dump_pagetable((pagetable_t)PTE2PA(pte), level - 1, va);
            }
        }
    }
}
