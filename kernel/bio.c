// 块缓存系统实现
#include "fs.h"
#include "riscv.h"

int printf(const char *fmt, ...);

// 模拟磁盘（使用内存数组）
static uint8 disk[FSSIZE * BSIZE];

// 块缓存结构
struct buf {
  int valid;       // 数据是否有效
  int dirty;       // 是否需要写回
  uint32 dev;      // 设备号
  uint32 blockno;  // 块号
  uint32 refcnt;   // 引用计数
  uint8 data[BSIZE];
};

#define NBUF 30
static struct buf bcache[NBUF];

// 初始化块缓存
void binit(void) {
  for (int i = 0; i < NBUF; i++) {
    bcache[i].valid = 0;
    bcache[i].dirty = 0;
    bcache[i].refcnt = 0;
  }
  printf("Block cache initialized (%d buffers)\n", NBUF);
}

// 查找或分配缓存块
static struct buf* bget(uint32 dev, uint32 blockno) {
  // 先查找是否已经在缓存中
  for (int i = 0; i < NBUF; i++) {
    if (bcache[i].valid && bcache[i].dev == dev && bcache[i].blockno == blockno) {
      bcache[i].refcnt++;
      return &bcache[i];
    }
  }
  
  // 没找到，分配一个空闲的缓存块
  for (int i = 0; i < NBUF; i++) {
    if (bcache[i].refcnt == 0) {
      // 如果是脏块，先写回
      if (bcache[i].dirty && bcache[i].valid) {
        uint32 offset = bcache[i].blockno * BSIZE;
        for (int j = 0; j < BSIZE; j++) {
          disk[offset + j] = bcache[i].data[j];
        }
        bcache[i].dirty = 0;
      }
      
      bcache[i].dev = dev;
      bcache[i].blockno = blockno;
      bcache[i].valid = 0;
      bcache[i].refcnt = 1;
      return &bcache[i];
    }
  }
  
  printf("bget: no buffers available\n");
  return 0;
}

// 读取块（返回缓存的块）
struct buf* bread(uint32 dev, uint32 blockno) {
  struct buf *b = bget(dev, blockno);
  if (!b) return 0;
  
  if (!b->valid) {
    // 从磁盘读取
    uint32 offset = blockno * BSIZE;
    for (int i = 0; i < BSIZE; i++) {
      b->data[i] = disk[offset + i];
    }
    b->valid = 1;
  }
  
  return b;
}

// 写入块（仅标记为脏，不立即写回）
void bwrite(struct buf *b) {
  b->dirty = 1;
}

// 释放块（减少引用计数）
void brelse(struct buf *b) {
  if (b->refcnt > 0) {
    b->refcnt--;
  }
}

// 将缓存中的所有脏块写回磁盘
void bsync(void) {
  for (int i = 0; i < NBUF; i++) {
    if (bcache[i].valid && bcache[i].dirty) {
      uint32 offset = bcache[i].blockno * BSIZE;
      for (int j = 0; j < BSIZE; j++) {
        disk[offset + j] = bcache[i].data[j];
      }
      bcache[i].dirty = 0;
    }
  }
}

// 获取磁盘指针（用于格式化）
uint8* get_disk_ptr(void) {
  return disk;
}

