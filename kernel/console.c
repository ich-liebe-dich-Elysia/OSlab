/*
 * 控制台功能实现 - 清屏和光标控制
 * 使用ANSI转义序列实现
 */

// 函数声明
void consputc(int c);
void uart_puts(const char *s);
int uartgetc(void);

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

// 向控制台写入数据
int consolewrite(char *buf, int n) {
    int i;
    for (i = 0; i < n; i++) {
        consputc(buf[i]);
    }
    return i;
}

// 从控制台读取数据
int consoleread(char *buf, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int c = uartgetc();
        if (c < 0)
            break;
        buf[i] = c;
        if (c == '\n' || c == '\r')
            break;
    }
    return i;
}
