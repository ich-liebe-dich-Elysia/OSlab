// 用户态系统调用接口

#ifndef USER_H
#define USER_H

// 基本类型定义
typedef unsigned long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef long int64;
typedef int int32;
typedef short int16;
typedef char int8;

// 进程控制
int fork(void);
int exit(int status) __attribute__((noreturn));
int wait(int *status);
int kill(int pid);
int getpid(void);

// 文件操作
int open(const char *path, int flags);
int close(int fd);
int read(int fd, void *buf, int n);
int write(int fd, const void *buf, int n);

// 内存管理
void* sbrk(int n);

// 其他
int sleep(int n);
int uptime(void);

// 标准I/O
#define stdin  0
#define stdout 1
#define stderr 2

#endif

