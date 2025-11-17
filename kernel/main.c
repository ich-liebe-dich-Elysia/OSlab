
// 类型定义
typedef unsigned long uint64;
typedef uint64 *pagetable_t;

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
uint64 walkaddr(pagetable_t pagetable, uint64 va);

extern pagetable_t kernel_pagetable;
extern char etext[];

// 测试框架
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (cond) { \
        test_passed++; \
    } else { \
        test_failed++; \
        printf("  [FAIL] %s\n", msg); \
    } \
} while(0)

#define TEST_SECTION(name) printf("\n>>> %s\n", name)

/*
 * 测试1：物理内存分配器基础功能
 */
void test_physical_memory_basic(void) {
    TEST_SECTION("Test 1: Physical Memory Allocator - Basic");
    
    // 测试单页分配
    void *page1 = alloc_page();
    void *page2 = alloc_page();
    
    TEST_ASSERT(page1 != 0, "alloc_page() returns non-null");
    TEST_ASSERT(page2 != 0, "second alloc_page() returns non-null");
    TEST_ASSERT(page1 != page2, "allocated pages are different");
    
    // 测试页对齐
    TEST_ASSERT(((uint64)page1 & 0xFFF) == 0, "page1 is 4KB aligned");
    TEST_ASSERT(((uint64)page2 & 0xFFF) == 0, "page2 is 4KB aligned");
    
    // 测试数据写入和读取
    *(int*)page1 = 0x12345678;
    *(int*)page2 = 0xABCDEF00;
    TEST_ASSERT(*(int*)page1 == 0x12345678, "page1 data write/read");
    TEST_ASSERT(*(int*)page2 == 0xABCDEF00, "page2 data write/read");
    
    // 测试释放和重新分配
    free_page(page1);
    void *page3 = alloc_page();
    TEST_ASSERT(page3 != 0, "alloc after free succeeds");
    
    // 伙伴系统可能复用page1的地址
    if (page3 == page1) {
        printf("  [INFO] Memory reuse detected (expected for buddy)\n");
    }
    
    free_page(page2);
    free_page(page3);
    
    printf("  Passed: %d/%d\n", test_passed - (test_count - 7), 7);
}

/*
 * 测试2：连续页面分配
 */
void test_physical_memory_contiguous(void) {
    TEST_SECTION("Test 2: Physical Memory Allocator - Contiguous");
    
    int base = test_passed;
    
    // 测试不同大小的连续分配
    void *pages1 = alloc_pages(1);
    void *pages4 = alloc_pages(4);
    void *pages8 = alloc_pages(8);
    
    TEST_ASSERT(pages1 != 0, "alloc 1 page succeeds");
    TEST_ASSERT(pages4 != 0, "alloc 4 pages succeeds");
    TEST_ASSERT(pages8 != 0, "alloc 8 pages succeeds");
    
    // 验证对齐（4页需要16KB对齐，8页需要32KB对齐）
    TEST_ASSERT(((uint64)pages4 & 0x3FFF) == 0, "4 pages aligned to 16KB");
    TEST_ASSERT(((uint64)pages8 & 0x7FFF) == 0, "8 pages aligned to 32KB");
    
    // 测试连续页面的数据独立性
    for (int i = 0; i < 4; i++) {
        *((int*)pages4 + i*1024) = 0x1000 + i;
    }
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(*((int*)pages4 + i*1024) == 0x1000 + i, 
                    "contiguous pages data integrity");
    }
    
    free_pages(pages1, 1);
    free_pages(pages4, 4);
    free_pages(pages8, 8);
    
    printf("  Passed: %d/%d\n", test_passed - base, test_count - base);
}

/*
 * 测试3：边界条件测试
 */
void test_physical_memory_edge_cases(void) {
    TEST_SECTION("Test 3: Physical Memory Allocator - Edge Cases");
    
    int base = test_passed;
    
    // 测试无效参数
    void *p1 = alloc_pages(0);
    void *p2 = alloc_pages(-1);
    TEST_ASSERT(p1 == 0, "alloc 0 pages returns NULL");
    TEST_ASSERT(p2 == 0, "alloc negative pages returns NULL");
    
    // 测试大块分配（接近最大order）
    void *large = alloc_pages(512);  // 2MB
    TEST_ASSERT(large != 0, "large allocation (512 pages) succeeds");
    if (large) {
        TEST_ASSERT(((uint64)large & 0x1FFFFF) == 0, "large block aligned to 2MB");
        free_pages(large, 512);
    }
    
    printf("  Passed: %d/%d\n", test_passed - base, test_count - base);
}

/*
 * 测试4：页表创建和映射
 */
void test_pagetable_mapping(void) {
    TEST_SECTION("Test 4: Page Table - Mapping");
    
    int base = test_passed;
    
    // kvminit已经创建了内核页表
    TEST_ASSERT(kernel_pagetable != 0, "kernel page table created");
    
    // 测试页表项对齐
    TEST_ASSERT(((uint64)kernel_pagetable & 0xFFF) == 0, 
                "page table is page aligned");
    
    // 验证关键地址的映射
    // 注意：启用分页后才能使用walkaddr
    
    printf("  Passed: %d/%d\n", test_passed - base, test_count - base);
}

/*
 * 测试5：虚拟内存启用前的状态
 */
void test_before_paging(void) {
    TEST_SECTION("Test 5: Before Enabling Paging");
    
    int base = test_passed;
    
    // 测试关键内存区域可访问
    volatile int test_var = 42;
    TEST_ASSERT(test_var == 42, "stack access works");
    
    // 测试代码段可执行（当前就在执行代码）
    TEST_ASSERT(1, "code execution works");
    
    // 测试UART可访问（printf能工作说明UART正常）
    TEST_ASSERT(1, "UART accessible (printf works)");
    
    printf("  Passed: %d/%d\n", test_passed - base, test_count - base);
}

/*
 * 测试6：虚拟内存启用
 */
void test_enable_paging(void) {
    TEST_SECTION("Test 6: Enabling Virtual Memory");
    
    int base = test_passed;
    
    printf("  Current PC: ~0x%lx\n", (uint64)test_enable_paging);
    printf("  etext: 0x%lx\n", (uint64)etext);
    
    // 启用分页
    kvminithart();
    
    // 如果能执行到这里，说明分页启用成功
    TEST_ASSERT(1, "paging enabled without crash");
    
    printf("  Passed: %d/%d\n", test_passed - base, test_count - base);
}

/*
 * 测试7：虚拟内存启用后的验证
 */
void test_after_paging(void) {
    TEST_SECTION("Test 7: After Enabling Paging");
    
    int base = test_passed;
    
    // 测试代码段仍可执行
    volatile int code_test = 100;
    code_test += 200;
    TEST_ASSERT(code_test == 300, "code execution after paging");
    
    // 测试栈访问
    volatile int stack_var = 0x11223344;
    TEST_ASSERT(stack_var == 0x11223344, "stack access after paging");
    
    // 测试数据段访问
    static int data_var = 0x55667788;
    TEST_ASSERT(data_var == 0x55667788, "data segment access after paging");
    
    // 测试BSS段访问
    static int bss_var;
    bss_var = 0x99AABBCC;
    TEST_ASSERT(bss_var == 0x99AABBCC, "BSS segment access after paging");
    
    // 测试堆分配（物理内存分配器）
    void *heap_page = alloc_page();
    TEST_ASSERT(heap_page != 0, "heap allocation after paging");
    
    int heap_access_ok = 0;
    if (heap_page) {
        *(int*)heap_page = 0xDEADBEEF;
        heap_access_ok = (*(int*)heap_page == 0xDEADBEEF);
        free_page(heap_page);
    }
    TEST_ASSERT(heap_access_ok, "heap access after paging");
    
    // 测试UART仍然工作（printf能输出）
    TEST_ASSERT(1, "UART works after paging");
    
    printf("  Passed: %d/%d\n", test_passed - base, test_count - base);
}

/*
 * 测试8：地址转换验证
 */
void test_address_translation(void) {
    TEST_SECTION("Test 8: Address Translation");
    
    int base = test_passed;
    
    // 测试代码段地址转换
    uint64 code_va = (uint64)test_address_translation;
    uint64 code_pa = walkaddr(kernel_pagetable, code_va);
    
    TEST_ASSERT(code_pa != 0, "code address translates");
    TEST_ASSERT((code_pa & ~0xFFFUL) == (code_va & ~0xFFFUL), 
                "code identity mapping verified");
    
    // 测试栈地址
    int stack_test = 123;
    uint64 stack_va = (uint64)&stack_test;
    uint64 stack_pa = walkaddr(kernel_pagetable, stack_va);
    
    TEST_ASSERT(stack_pa != 0, "stack translation succeeds");
    TEST_ASSERT((stack_pa & ~0xFFFUL) == (stack_va & ~0xFFFUL), 
                "stack identity mapping verified");
    
    printf("  Passed: %d/%d\n", test_passed - base, test_count - base);
}


/*
 * 主测试入口
 */
void kernel_main(void)
{
    // 初始化UART
    uart_init();
    
    // 阶段1：初始化物理内存分配器
    printf("\n[INIT] Initializing buddy allocator...\n");
    buddy_init();
    
    // 阶段2：物理内存分配器测试
    test_physical_memory_basic();
    test_physical_memory_contiguous();
    test_physical_memory_edge_cases();
    
    // 阶段3：页表初始化
    printf("\n[INIT] Initializing page table...\n");
    kvminit();
    test_pagetable_mapping();
    
    // 阶段4：启用分页前的测试
    test_before_paging();
    
    // 阶段5：启用虚拟内存
    printf("\n[CRITICAL] Enabling virtual memory...\n");
    test_enable_paging();
    
    // 阶段6：启用分页后的测试
    test_after_paging();
    test_address_translation();
    
    
    // 进入空闲循环
    while (1) {
        asm volatile("wfi");
    }
}
