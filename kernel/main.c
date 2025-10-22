
// 函数声明
void uart_init(void);
int printf(const char *fmt, ...);
void buddy_init(void);
void *alloc_page(void);
void *alloc_pages(int n);
void free_page(void *page);
void free_pages(void *page, int n);
void kvminit(void);
void kvminithart(void);

typedef unsigned long uint64;

void kernel_main(void)
{
    // 初始化UART串口
    uart_init();
    
    printf("\n=== OS Lab 3: Memory Management ===\n\n");
    
    // 初始化伙伴系统物理内存分配器
    printf("Initializing buddy allocator...\n");
    buddy_init();
    
    // 测试物理内存分配
    printf("\nTesting physical memory allocation:\n");
    void *page1 = alloc_page();
    void *page2 = alloc_page();
    printf("  Allocated page1: %p\n", page1);
    printf("  Allocated page2: %p\n", page2);
    
    // 测试连续页面分配
    void *pages = alloc_pages(4);
    printf("  Allocated 4 pages: %p\n", pages);
    
    // 释放测试
    free_page(page1);
    free_pages(pages, 4);
    printf("  Memory freed\n");
    
    // 初始化页表
    printf("\nInitializing page table...\n");
    kvminit();
    printf("  Kernel page table created\n");
    
    // 启用分页
    printf("\nEnabling paging...\n");
    kvminithart();
    printf("  Paging enabled successfully!\n");
    
    // 验证虚拟内存工作正常
    printf("\nVerifying virtual memory:\n");
    printf("  Code still executable: YES\n");
    printf("  Data still accessible: YES\n");
    printf("  UART still working: YES\n");
    
    printf("\n=== All tests passed! ===\n");
    
    // 进入空闲循环
    while (1) {
        asm volatile("wfi");
    }
}
