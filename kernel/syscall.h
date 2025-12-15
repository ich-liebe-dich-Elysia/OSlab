// 系统调用号定义

#ifndef SYSCALL_H
#define SYSCALL_H

// 进程控制类
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_kill    4
#define SYS_getpid  5

// 文件操作类
#define SYS_open    6
#define SYS_close   7
#define SYS_read    8
#define SYS_write   9

// 内存管理类
#define SYS_sbrk    10

// 其他
#define SYS_sleep   11
#define SYS_uptime  12

#endif

