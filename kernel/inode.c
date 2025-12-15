// inode管理实现
#include "fs.h"

int printf(const char *fmt, ...);
extern struct buf* bread(uint32 dev, uint32 blockno);
extern void bwrite(struct buf *b);
extern void brelse(struct buf *b);
extern void log_write(struct buf *b);
extern void begin_op(void);
extern void end_op(void);

// 内存中的inode缓存
#define NINODE 50
static struct inode icache[NINODE];
static struct superblock sb;

// 初始化inode系统
void iinit(int dev) {
  // 读取超级块
  struct buf *bp = bread(dev, SUPERBLOCK);
  struct superblock *sb_disk = (struct superblock *)bp->data;
  sb = *sb_disk;
  brelse(bp);
  
  // 初始化inode缓存
  for (int i = 0; i < NINODE; i++) {
    icache[i].ref = 0;
    icache[i].valid = 0;
  }
  
  printf("Inode system initialized (ninodes=%d)\n", sb.ninodes);
}

// 分配一个新的inode
struct inode* ialloc(uint32 dev, uint16 type) {
  // 遍历所有inode，找到一个空闲的
  for (uint32 inum = 1; inum <= sb.ninodes; inum++) {
    struct buf *bp = bread(dev, IBLOCK(inum, sb));
    struct dinode *dip = (struct dinode*)bp->data + inum % IPB;
    
    if (dip->type == 0) {  // 空闲inode
      // 初始化
      for (int i = 0; i < sizeof(struct dinode); i++) {
        ((uint8*)dip)[i] = 0;
      }
      dip->type = type;
      log_write(bp);
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  
  printf("ialloc: no free inodes\n");
  return 0;
}

// 获取inode（增加引用计数）
struct inode* iget(uint32 dev, uint32 inum) {
  // 先查找是否已经在缓存中
  for (int i = 0; i < NINODE; i++) {
    if (icache[i].ref > 0 && icache[i].inum == inum && icache[i].dev == dev) {
      icache[i].ref++;
      return &icache[i];
    }
  }
  
  // 找一个空闲的缓存项
  for (int i = 0; i < NINODE; i++) {
    if (icache[i].ref == 0) {
      icache[i].dev = dev;
      icache[i].inum = inum;
      icache[i].ref = 1;
      icache[i].valid = 0;
      return &icache[i];
    }
  }
  
  printf("iget: no free inodes in cache\n");
  return 0;
}

// 从磁盘加载inode数据
static void iload(struct inode *ip) {
  if (!ip->valid) {
    struct buf *bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    struct dinode *dip = (struct dinode*)bp->data + ip->inum % IPB;
    
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    for (int i = 0; i < NDIRECT+1; i++) {
      ip->addrs[i] = dip->addrs[i];
    }
    
    brelse(bp);
    ip->valid = 1;
  }
}

// 将inode数据写回磁盘
void iupdate(struct inode *ip) {
  struct buf *bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  struct dinode *dip = (struct dinode*)bp->data + ip->inum % IPB;
  
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  for (int i = 0; i < NDIRECT+1; i++) {
    dip->addrs[i] = ip->addrs[i];
  }
  
  log_write(bp);
  brelse(bp);
}

// 释放inode（减少引用计数）
void iput(struct inode *ip) {
  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    // 最后一个引用且没有链接，释放inode
    
    // 释放数据块
    itrunc(ip);
    
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;
  }
  
  ip->ref--;
}

// 锁定inode（加载数据）
void ilock(struct inode *ip) {
  if (ip == 0 || ip->ref < 1) {
    return;
  }
  
  iload(ip);
  
  if (ip->type == 0) {
    printf("ilock: no type\n");
  }
}

// 解锁inode
void iunlock(struct inode *ip) {
  // 简化实现：不需要真正的锁
}

// 分配数据块
static uint32 balloc(uint32 dev) {
  // 遍历位图，找空闲块
  for (uint32 b = 0; b < sb.size; b++) {
    struct buf *bp = bread(dev, BBLOCK(b, sb));
    uint8 *bits = (uint8*)bp->data;
    int bi = b % BPB;
    int m = 1 << (bi % 8);
    
    if ((bits[bi/8] & m) == 0) {  // 找到空闲块
      bits[bi/8] |= m;
      log_write(bp);
      brelse(bp);
      
      // 清零新分配的块
      struct buf *zbuf = bread(dev, b);
      for (int i = 0; i < BSIZE; i++) {
        zbuf->data[i] = 0;
      }
      log_write(zbuf);
      brelse(zbuf);
      
      return b;
    }
    brelse(bp);
  }
  
  printf("balloc: out of space\n");
  return 0;
}

// 释放数据块
static void bfree(uint32 dev, uint32 b) {
  struct buf *bp = bread(dev, BBLOCK(b, sb));
  uint8 *bits = (uint8*)bp->data;
  int bi = b % BPB;
  int m = 1 << (bi % 8);
  bits[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// 获取inode的第n个数据块（分配）
static uint32 bmap(struct inode *ip, uint32 bn) {
  uint32 addr;
  
  if (bn < NDIRECT) {
    // 直接块
    if ((addr = ip->addrs[bn]) == 0) {
      ip->addrs[bn] = addr = balloc(ip->dev);
    }
    return addr;
  }
  bn -= NDIRECT;
  
  if (bn < NINDIRECT) {
    // 间接块
    if ((addr = ip->addrs[NDIRECT]) == 0) {
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    }
    struct buf *bp = bread(ip->dev, addr);
    uint32 *a = (uint32*)bp->data;
    if ((addr = a[bn]) == 0) {
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  
  printf("bmap: out of range\n");
  return 0;
}

// 截断inode（释放所有数据块）
void itrunc(struct inode *ip) {
  // 释放直接块
  for (int i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }
  
  // 释放间接块
  if (ip->addrs[NDIRECT]) {
    struct buf *bp = bread(ip->dev, ip->addrs[NDIRECT]);
    uint32 *a = (uint32*)bp->data;
    for (int j = 0; j < NINDIRECT; j++) {
      if (a[j]) {
        bfree(ip->dev, a[j]);
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }
  
  ip->size = 0;
  iupdate(ip);
}

// 读取inode数据
int readi(struct inode *ip, uint8 *dst, uint32 off, uint32 n) {
  if (off > ip->size || off + n < off) {
    return 0;
  }
  if (off + n > ip->size) {
    n = ip->size - off;
  }
  
  uint32 tot = 0;
  while (tot < n) {
    uint32 addr = bmap(ip, off / BSIZE);
    if (addr == 0) {
      break;
    }
    struct buf *bp = bread(ip->dev, addr);
    uint32 m = (n - tot < BSIZE - off % BSIZE) ? n - tot : BSIZE - off % BSIZE;
    for (uint32 i = 0; i < m; i++) {
      dst[tot + i] = bp->data[off % BSIZE + i];
    }
    brelse(bp);
    tot += m;
    off += m;
  }
  
  return tot;
}

// 写入inode数据
int writei(struct inode *ip, uint8 *src, uint32 off, uint32 n) {
  if (off > ip->size || off + n < off) {
    return 0;
  }
  if (off + n > MAXFILE * BSIZE) {
    return 0;
  }
  
  uint32 tot = 0;
  while (tot < n) {
    uint32 addr = bmap(ip, off / BSIZE);
    if (addr == 0) {
      break;
    }
    struct buf *bp = bread(ip->dev, addr);
    uint32 m = (n - tot < BSIZE - off % BSIZE) ? n - tot : BSIZE - off % BSIZE;
    for (uint32 i = 0; i < m; i++) {
      bp->data[off % BSIZE + i] = src[tot + i];
    }
    log_write(bp);
    brelse(bp);
    tot += m;
    off += m;
  }
  
  if (off > ip->size) {
    ip->size = off;
  }
  iupdate(ip);
  
  return tot;
}

