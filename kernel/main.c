
// 函数声明
void uart_init(void);
void uart_puts(const char *s);
int printf(const char *fmt, ...);
void clear_screen(void);

// 用于测试BSS段清零的全局变量
int bss_test_var;

// 延时函数
void delay(void)
{
    for (volatile int i = 0; i < 10000000; i++);
}

/*
 * 从entry.S汇编代码跳转到此处
 */
void kernel_main(void)
{
    // 初始化UART串口
    uart_init();
    
    // 测试printf功能
    printf("Integer: %d\n", 42);
    printf("Negative: %d\n", -123);
    printf("Hex: %x\n", 255);
    printf("Pointer: %p\n", (void*)0x80000000);
    printf("String: %s\n", "Hello World");
    printf("Character: %c\n", 'A');
    printf("Percent: %%\n");
    delay();
    // 清屏
    clear_screen();

    printf("INT_MAX: %d\n", 2147483647);
    printf("INT_MIN: %d\n", -2147483648);
    printf("NULL string: %s\n", (char*)0);
    printf("Empty string: %s\n", "");
    // 进入空闲循环
    while (1) {
        asm volatile("wfi");
    }
}
