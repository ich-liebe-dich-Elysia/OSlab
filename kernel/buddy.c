/*
 * 伙伴算法物理内存分配器
 */

typedef unsigned long uint64;
typedef unsigned int uint32;

// 函数声明
int printf(const char *fmt, ...);
void *memset(void *dst, int c, uint64 n);

// 内存布局常量
#define PGSIZE 4096
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)  // 128MB

// 伙伴系统配置
#define MAX_ORDER 10  // 支持最大2^10 = 1024页 = 4MB
#define MIN_ORDER 0   // 最小1页 = 4KB

// 空闲块链表节点
struct free_area {
    struct free_block *free_list;
    unsigned int nr_free;  // 当前order的空闲块数量
};

struct free_block {
    struct free_block *next;
};

// 全局伙伴系统数据
static struct {
    struct free_area free_area[MAX_ORDER + 1];
    uint64 mem_start;  // 可用内存起始地址
    uint64 mem_end;    // 可用内存结束地址
    uint64 total_pages;
} buddy;

// 外部符号（由链接脚本定义）
extern char end[];

// 计算2的幂
static inline int is_power_of_2(unsigned int n) {
    return n && !(n & (n - 1));
}

// 计算order（向上取整到2的幂）
static int size_to_order(int npages) {
    int order = 0;
    int size = 1;
    
    while (size < npages && order < MAX_ORDER) {
        size <<= 1;
        order++;
    }
    
    return order;
}

// 计算伙伴块的地址
static inline uint64 get_buddy_pfn(uint64 pfn, int order) {
    return pfn ^ (1UL << order);
}

/*
 * 初始化伙伴系统
 */
void buddy_init(void) {
    // 初始化所有空闲链表
    for (int i = 0; i <= MAX_ORDER; i++) {
        buddy.free_area[i].free_list = 0;
        buddy.free_area[i].nr_free = 0;
    }
    
    // 确定可用内存范围（对齐到页边界）
    buddy.mem_start = ((uint64)end + PGSIZE - 1) & ~(PGSIZE - 1);
    buddy.mem_end = PHYSTOP;
    buddy.total_pages = (buddy.mem_end - buddy.mem_start) / PGSIZE;
    
    printf("Buddy system: %d KB memory available\n", 
           (int)(buddy.total_pages * PGSIZE / 1024));
    
    // 将所有内存按最大块加入伙伴系统
    uint64 addr = buddy.mem_start;
    while (addr + PGSIZE <= buddy.mem_end) {
        int order = MAX_ORDER;
        uint64 size = (1UL << order) * PGSIZE;
        
        // 找到当前地址能分配的最大order  
        while (order > 0 && (addr + size > buddy.mem_end || (addr & (size - 1)))) {
            order--;
            size = (1UL << order) * PGSIZE;
        }
        
        if (order >= 0 && addr + size <= buddy.mem_end) {
            // 加入对应order的空闲链表
            struct free_block *block = (struct free_block *)addr;
            block->next = buddy.free_area[order].free_list;
            buddy.free_area[order].free_list = block;
            buddy.free_area[order].nr_free++;
            
            addr += size;
        } else {
            break;
        }
    }
}

/*
 * 分配2^order个连续页面
 */
static void *buddy_alloc_pages(int order) {
    if (order > MAX_ORDER || order < 0)
        return 0;
    
    // 在当前order查找空闲块
    int current_order = order;
    while (current_order <= MAX_ORDER) {
        if (buddy.free_area[current_order].free_list) {
            // 找到空闲块
            struct free_block *block = buddy.free_area[current_order].free_list;
            buddy.free_area[current_order].free_list = block->next;
            buddy.free_area[current_order].nr_free--;
            
            // 如果块太大，进行分裂
            while (current_order > order) {
                current_order--;
                uint64 buddy_addr = (uint64)block + ((1UL << current_order) * PGSIZE);
                struct free_block *buddy_block = (struct free_block *)buddy_addr;
                
                // 将分裂出的伙伴块加入空闲链表
                buddy_block->next = buddy.free_area[current_order].free_list;
                buddy.free_area[current_order].free_list = buddy_block;
                buddy.free_area[current_order].nr_free++;
            }
            
            // 清零分配的页面
            memset(block, 0, (1UL << order) * PGSIZE);
            return block;
        }
        current_order++;
    }
    
    return 0;  // 分配失败
}

/*
 * 释放2^order个连续页面
 */
static void buddy_free_pages(void *addr, int order) {
    if (order > MAX_ORDER || order < 0)
        return;
    
    if ((uint64)addr < buddy.mem_start || (uint64)addr >= buddy.mem_end)
        return;
    
    if ((uint64)addr & ((1UL << order) * PGSIZE - 1))
        return;  // 地址未对齐
    
    uint64 pfn = ((uint64)addr - buddy.mem_start) / PGSIZE;
    
    // 尝试合并伙伴块
    while (order < MAX_ORDER) {
        uint64 buddy_pfn = get_buddy_pfn(pfn, order);
        uint64 buddy_addr = buddy.mem_start + buddy_pfn * PGSIZE;
        
        // 检查伙伴块是否存在且空闲
        struct free_block **prev = &buddy.free_area[order].free_list;
        struct free_block *curr = *prev;
        int found = 0;
        
        while (curr) {
            if ((uint64)curr == buddy_addr) {
                // 找到伙伴块，进行合并
                *prev = curr->next;
                buddy.free_area[order].nr_free--;
                found = 1;
                
                // 合并后的块地址取两者较小值
                if (buddy_pfn < pfn) {
                    addr = (void *)buddy_addr;
                    pfn = buddy_pfn;
                }
                
                order++;
                break;
            }
            prev = &curr->next;
            curr = curr->next;
        }
        
        if (!found)
            break;  // 伙伴块不空闲，停止合并
    }
    
    // 将块加入对应order的空闲链表
    struct free_block *block = (struct free_block *)addr;
    block->next = buddy.free_area[order].free_list;
    buddy.free_area[order].free_list = block;
    buddy.free_area[order].nr_free++;
}

/*
 * 分配单个页面（4KB）
 */
void *alloc_page(void) {
    return buddy_alloc_pages(0);
}

/*
 * 分配n个连续页面
 */
void *alloc_pages(int n) {
    if (n <= 0)
        return 0;
    
    int order = size_to_order(n);
    return buddy_alloc_pages(order);
}

/*
 * 释放单个页面
 */
void free_page(void *page) {
    buddy_free_pages(page, 0);
}

/*
 * 释放n个连续页面
 */
void free_pages(void *page, int n) {
    if (n <= 0)
        return;
    
    int order = size_to_order(n);
    buddy_free_pages(page, order);
}

/*
 * 内存统计信息
 */
void buddy_stats(void) {
    printf("Buddy system statistics:\n");
    for (int i = 0; i <= MAX_ORDER; i++) {
        if (buddy.free_area[i].nr_free > 0) {
            printf("  Order %d: %d blocks (%d KB each)\n",
                   i, buddy.free_area[i].nr_free, 
                   (1 << i) * PGSIZE / 1024);
        }
    }
}

// memset实现
void *memset(void *dst, int c, uint64 n) {
    char *cdst = (char *)dst;
    for (uint64 i = 0; i < n; i++) {
        cdst[i] = c;
    }
    return dst;
}
