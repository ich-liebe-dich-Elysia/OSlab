/*
 * 内核printf实现
 * 实现基本的格式化输出功能
 */

// 可变参数支持
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

// 函数声明
void consputc(int c);

// 数字转字符串的字符表
static char digits[] = "0123456789abcdef";

/*
 * 数字转换算法
 * 处理不同进制的数字输出
 */
static void printint(long long xx, int base, int sign)
{
    char buf[20];  // 足够存储64位数字
    int i;
    unsigned long long x;

    // 处理负数
    if (sign && (sign = (xx < 0)))
        x = -xx;
    else
        x = xx;

    // 数字转换 - 逆序存储避免递归
    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    // 添加负号
    if (sign)
        buf[i++] = '-';

    // 逆序输出
    while (--i >= 0)
        consputc(buf[i]);
}

/*
 * 指针输出 
 */
static void printptr(unsigned long long x)
{
    int i;
    consputc('0');
    consputc('x');
    for (i = 0; i < 16; i++, x <<= 4)
        consputc(digits[x >> 60]);
}

/*
 * 内核printf
 * 支持的格式：%d, %x, %p, %c, %s, %%
 */
int printf(const char *fmt, ...)
{
    va_list ap;
    int i, c;
    char *s;

    va_start(ap, fmt);
    
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            consputc(c);
            continue;
        }
        
        // 处理格式符
        i++;
        c = fmt[i] & 0xff;
        
        switch (c) {
        case 'd':
            printint(va_arg(ap, int), 10, 1);
            break;
        case 'x':
            printint(va_arg(ap, int), 16, 0);
            break;
        case 'p':
            printptr(va_arg(ap, unsigned long long));
            break;
        case 'c':
            consputc(va_arg(ap, int));
            break;
        case 's':
            if ((s = va_arg(ap, char*)) == 0)
                s = "(null)";
            for (; *s; s++)
                consputc(*s);
            break;
        case '%':
            consputc('%');
            break;
        case 0:
            goto done;
        default:
            // 未知格式符 - 显示以引起注意
            consputc('%');
            consputc(c);
            break;
        }
    }
    
done:
    va_end(ap);
    return 0;
}
