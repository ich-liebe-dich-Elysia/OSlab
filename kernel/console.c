/*
 * 控制台功能实现 - 清屏和光标控制
 * 使用ANSI转义序列实现
 */

// 函数声明
void consputc(int c);
void uart_puts(const char *s);

/*
 * 清屏功能 - 输出足够多的换行符
 * 
 */
void clear_screen(void)
{
    // 输出50行换行符，将之前的内容推出屏幕
    for (int i = 0; i < 50; i++) {
        consputc('\n');
    }
}

void goto_xy(int x, int y)
{
    // 输出ANSI序列
    consputc('\033');
    consputc('[');
    
    // 输出行号
    if (y >= 10) {
        consputc('0' + y / 10);
    }
    consputc('0' + y % 10);
    
    consputc(';');
    
    // 输出列号
    if (x >= 10) {
        consputc('0' + x / 10);
    }
    consputc('0' + x % 10);
    
    consputc('H');
}
