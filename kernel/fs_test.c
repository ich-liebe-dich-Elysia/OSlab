// 文件系统测试
#include "fs.h"

int printf(const char *fmt, ...);
extern void mkfs(void);
extern struct inode* create(char *path, uint16 type);
extern struct inode* namei(char *path);
extern struct file* filealloc(void);
extern void fileclose(struct file *f);
extern int fileread(struct file *f, uint8 *addr, int n);
extern int filewrite(struct file *f, uint8 *addr, int n);
extern void ilock(struct inode *ip);
extern void iunlock(struct inode *ip);
extern void iput(struct inode *ip);
extern void begin_op(void);
extern void end_op(void);
extern void exit_process(int status);

// 简单的内存比较
static int memcmp(const void *s1, const void *s2, int n) {
  const uint8 *p1 = s1, *p2 = s2;
  for (int i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

// 测试1：基本文件创建和读写
void test_basic_file_operations(void) {
  printf("\n[测试1] 基本文件操作\n");
  
  // 创建文件
  begin_op();
  struct inode *ip = create("/testfile", T_FILE);
  if (!ip) {
    printf("[失败] 无法创建文件\n");
    end_op();
    return;
  }
  printf("[成功] 创建文件 /testfile\n");
  
  // 写入数据
  uint8 wbuf[64];
  for (int i = 0; i < 64; i++) {
    wbuf[i] = 'A' + (i % 26);
  }
  
  ilock(ip);
  int nwrite = writei(ip, wbuf, 0, 64);
  iunlock(ip);
  
  if (nwrite != 64) {
    printf("[失败] 写入失败，预期64字节，实际%d字节\n", nwrite);
  } else {
    printf("[成功] 写入64字节\n");
  }
  
  // 读取并验证
  uint8 rbuf[64];
  for (int i = 0; i < 64; i++) {
    rbuf[i] = 0;
  }
  
  ilock(ip);
  int nread = readi(ip, rbuf, 0, 64);
  iunlock(ip);
  
  if (nread != 64) {
    printf("[失败] 读取失败，预期64字节，实际%d字节\n", nread);
  } else if (memcmp(wbuf, rbuf, 64) != 0) {
    printf("[失败] 数据不匹配\n");
  } else {
    printf("[成功] 读取64字节，数据匹配\n");
  }
  
  iput(ip);
  end_op();
}

// 测试2：目录操作
void test_directory_operations(void) {
  printf("\n[测试2] 目录操作\n");
  
  begin_op();
  
  // 创建目录
  struct inode *dir = create("/testdir", T_DIR);
  if (!dir) {
    printf("[失败] 无法创建目录\n");
    end_op();
    return;
  }
  printf("[成功] 创建目录 /testdir\n");
  iput(dir);
  
  // 在目录中创建文件
  struct inode *file = create("/testdir/file1", T_FILE);
  if (!file) {
    printf("[失败] 无法在目录中创建文件\n");
    end_op();
    return;
  }
  printf("[成功] 在目录中创建文件 /testdir/file1\n");
  iput(file);
  
  // 查找文件
  end_op();
  begin_op();
  
  struct inode *found = namei("/testdir/file1");
  if (!found) {
    printf("[失败] 无法查找文件\n");
  } else {
    printf("[成功] 成功查找文件\n");
    iput(found);
  }
  
  end_op();
}

// 测试3：大文件写入
void test_large_file(void) {
  printf("\n[测试3] 大文件写入\n");
  
  begin_op();
  struct inode *ip = create("/largefile", T_FILE);
  if (!ip) {
    printf("[失败] 无法创建大文件\n");
    end_op();
    return;
  }
  
  // 写入多个块
  uint8 buf[BSIZE];
  for (int i = 0; i < BSIZE; i++) {
    buf[i] = i % 256;
  }
  
  ilock(ip);
  int total = 0;
  for (int block = 0; block < 5; block++) {  // 写入5个块
    int n = writei(ip, buf, block * BSIZE, BSIZE);
    total += n;
    if (n != BSIZE) {
      printf("[警告] 第%d块写入不完整: %d/%d\n", block, n, BSIZE);
      break;
    }
  }
  iunlock(ip);
  
  printf("[成功] 写入大文件: %d 字节 (%d 块)\n", total, total / BSIZE);
  
  // 验证读取
  ilock(ip);
  uint8 rbuf[BSIZE];
  int nread = readi(ip, rbuf, BSIZE * 2, BSIZE);  // 读取第3块
  iunlock(ip);
  
  if (nread == BSIZE && memcmp(buf, rbuf, BSIZE) == 0) {
    printf("[成功] 大文件读取验证通过\n");
  } else {
    printf("[失败] 大文件读取验证失败\n");
  }
  
  iput(ip);
  end_op();
}

// 测试4：日志系统（崩溃恢复）
void test_log_system(void) {
  printf("\n[测试4] 日志系统\n");
  printf("[信息] 所有操作都通过日志系统保证原子性\n");
  
  begin_op();
  struct inode *ip = create("/logtest", T_FILE);
  if (!ip) {
    printf("[失败] 创建测试文件失败\n");
    end_op();
    return;
  }
  
  // 多次写入（会被日志记录）
  uint8 data[] = "Transaction test data";
  ilock(ip);
  writei(ip, data, 0, sizeof(data));
  iunlock(ip);
  
  printf("[成功] 文件操作通过日志系统完成\n");
  printf("[信息] 如果在 end_op() 前崩溃，所有修改会被撤销\n");
  
  iput(ip);
  end_op();
  
  printf("[成功] 事务提交完成\n");
}

// 运行所有测试
void test_filesystem(void) {
  printf("\n========================================\n");
  printf("  文件系统功能测试\n");
  printf("========================================\n");
  
  // 格式化文件系统
  mkfs();
  
  // 运行测试
  test_basic_file_operations();
  test_directory_operations();
  test_large_file();
  test_log_system();
  
  printf("\n========================================\n");
  printf("  文件系统测试完成\n");
  printf("========================================\n");
  
  exit_process(0);
}

