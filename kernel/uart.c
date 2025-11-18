/*
 * 最小UART驱动 - 参考xv6实现
 * 只保留OS启动过程必需的串口输出功能
 */

// UART 16550 寄存器定义（参考xv6/uart.c）
#define UART0_BASE  0x10000000L

// 寄存器偏移
#define THR 0    // Transmit Holding Register
#define LSR 5    // Line Status Register  
#define LCR 3    // Line Control Register
#define IER 1    // Interrupt Enable Register

// 寄存器位定义
#define LSR_TX_IDLE (1<<5)      // 发送缓冲区空闲
#define LSR_RX_READY (1<<0)     // 接收数据就绪
#define LCR_EIGHT_BITS (3<<0)   // 8位数据
#define LCR_BAUD_LATCH (1<<7)   // 波特率设置模式
#define IER_RX_ENABLE (1<<0)    // 接收中断使能
#define IER_TX_ENABLE (1<<1)    // 发送中断使能

// 寄存器访问宏
#define REG(offset) ((volatile unsigned char *)(UART0_BASE + (offset)))
#define write_reg(offset, value) (*REG(offset) = (value))
#define read_reg(offset) (*REG(offset))

/*
 * 初始化UART - 参考xv6的uartinit()
 * 配置基本的串口输出功能
 */
void uart_init(void)
{
    // 禁用中断
    write_reg(IER, 0x00);
    
    // 设置波特率（进入波特率设置模式）
    write_reg(LCR, LCR_BAUD_LATCH);
    write_reg(0, 0x03);  // 波特率低8位
    write_reg(1, 0x00);  // 波特率高8位
    
    // 配置数据格式：8位数据，无校验，1停止位
    write_reg(LCR, LCR_EIGHT_BITS);
}

/*
 * 输出单个字符
 */
static void uart_putc(char c)
{
    // 等待发送缓冲区空闲
    while ((read_reg(LSR) & LSR_TX_IDLE) == 0) {
        // 自旋等待
    }
    
    // 发送字符
    write_reg(THR, c);
}

/*
 * 输出字符串 - OS启动过程的主要输出接口
 */
void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');  // 换行前先发送回车
        }
        uart_putc(*s);
        s++;
    }
}

/*
 * 控制台字符输出接口 - 为printf提供统一接口
 */
void consputc(int c)
{
    uart_putc(c);
}

/*
 * 读取一个字符（轮询方式）
 */
int uart_getc(void)
{
    if (read_reg(LSR) & LSR_RX_READY) {
        return read_reg(0);  // RHR (Receive Holding Register)
    }
    return -1;  // 没有数据
}

/*
 * 使能UART接收中断
 */
void uart_enable_rx_interrupt(void)
{
    write_reg(IER, IER_RX_ENABLE);
}

/*
 * UART中断处理函数
 */
void uart_intr(void)
{
    // 读取所有可用字符
    while (1) {
        int c = uart_getc();
        if (c == -1) {
            break;
        }
        
        // 简单回显
        uart_putc(c);
        
        // 特殊字符处理
        if (c == '\r' || c == '\n') {
            uart_putc('\n');
        }
    }
}
