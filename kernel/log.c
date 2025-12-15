// 日志系统实现（写前日志）
#include "fs.h"

int printf(const char *fmt, ...);
extern struct buf* bread(uint32 dev, uint32 blockno);
extern void bwrite(struct buf *b);
extern void brelse(struct buf *b);
extern void bsync(void);

// 日志头结构（存储在日志的第一个块）
struct logheader {
  int n;                    // 日志中的块数
  int block[LOGSIZE - 1];   // 每个日志块对应的实际块号
};

// 日志状态
struct log {
  int dev;
  int start;                // 日志区起始块号
  int size;                 // 日志区大小
  int outstanding;          // 当前未完成的操作数
  int committing;           // 是否正在提交
  
  struct logheader lh;      // 内存中的日志头
};

static struct log log;

// 从磁盘读取日志头
static void read_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *)(buf->data);
  log.lh.n = lh->n;
  for (int i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// 将日志头写入磁盘
static void write_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *)(buf->data);
  hb->n = log.lh.n;
  for (int i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

// 从日志恢复
static void recover_from_log(void) {
  read_head();
  if (log.lh.n > 0) {
    // 将日志中的数据复制到实际位置
    for (int i = 0; i < log.lh.n; i++) {
      struct buf *lbuf = bread(log.dev, log.start + 1 + i);  // 日志块
      struct buf *dbuf = bread(log.dev, log.lh.block[i]);    // 目标块
      
      // 复制数据
      for (int j = 0; j < BSIZE; j++) {
        dbuf->data[j] = lbuf->data[j];
      }
      bwrite(dbuf);
      brelse(lbuf);
      brelse(dbuf);
    }
    
    // 清空日志
    log.lh.n = 0;
    write_head();
  }
  bsync();
}

// 初始化日志系统
void loginit(int dev, struct superblock *sb) {
  log.dev = dev;
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.outstanding = 0;
  log.committing = 0;
  
  // 从崩溃中恢复
  recover_from_log();
  
  printf("Log system initialized (start=%d, size=%d)\n", log.start, log.size);
}

// 将缓存中的日志数据写入磁盘日志区
static void write_log(void) {
  for (int i = 0; i < log.lh.n; i++) {
    struct buf *to = bread(log.dev, log.start + 1 + i);    // 日志块
    struct buf *from = bread(log.dev, log.lh.block[i]);    // 数据块
    
    // 复制数据到日志
    for (int j = 0; j < BSIZE; j++) {
      to->data[j] = from->data[j];
    }
    bwrite(to);
    brelse(from);
    brelse(to);
  }
}

// 提交日志（将日志写入实际位置）
static void commit(void) {
  if (log.lh.n > 0) {
    write_log();           // 1. 将数据写入日志区
    write_head();          // 2. 写入日志头（提交点）
    bsync();               // 3. 确保写入磁盘
    
    // 4. 将日志数据复制到实际位置
    for (int i = 0; i < log.lh.n; i++) {
      struct buf *lbuf = bread(log.dev, log.start + 1 + i);
      struct buf *dbuf = bread(log.dev, log.lh.block[i]);
      
      for (int j = 0; j < BSIZE; j++) {
        dbuf->data[j] = lbuf->data[j];
      }
      bwrite(dbuf);
      brelse(lbuf);
      brelse(dbuf);
    }
    
    // 5. 清空日志
    log.lh.n = 0;
    write_head();
    bsync();
  }
}

// 开始一个文件系统操作
void begin_op(void) {
  log.outstanding++;
}

// 结束文件系统操作
void end_op(void) {
  log.outstanding--;
  
  if (log.outstanding == 0) {
    log.committing = 1;
    commit();
    log.committing = 0;
  }
}

// 记录一个块的修改（在日志中记录）
void log_write(struct buf *b) {
  // 检查是否已经在日志中
  int i;
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno) {
      break;
    }
  }
  
  // 如果不在，添加到日志
  if (i == log.lh.n) {
    if (log.lh.n >= LOGSIZE - 1) {
      printf("log_write: transaction too big\n");
      return;
    }
    log.lh.block[log.lh.n++] = b->blockno;
  }
  
  bwrite(b);  // 标记为脏
}

