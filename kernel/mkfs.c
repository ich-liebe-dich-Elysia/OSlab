// 文件系统格式化工具
#include "fs.h"

int printf(const char *fmt, ...);
extern uint8* get_disk_ptr(void);
extern void binit(void);
extern void loginit(int dev, struct superblock *sb);
extern void iinit(int dev);
extern struct inode* ialloc(uint32 dev, uint16 type);
extern void ilock(struct inode *ip);
extern void iunlock(struct inode *ip);
extern void iput(struct inode *ip);
extern int dirlink(struct inode *dp, char *name, uint32 inum);
extern void begin_op(void);
extern void end_op(void);

// 格式化文件系统
void mkfs(void) {
  printf("\n=== Formatting filesystem ===\n");
  
  uint8 *disk = get_disk_ptr();
  
  // 清零整个磁盘
  for (int i = 0; i < FSSIZE * BSIZE; i++) {
    disk[i] = 0;
  }
  
  // 计算各区域大小
  uint32 ninodeblocks = NINODES / IPB + 1;
  uint32 nbitmap = FSSIZE / (BSIZE * 8) + 1;
  uint32 nmeta = 2 + LOGSIZE + ninodeblocks + nbitmap;  // boot + super + log + inode + bitmap
  uint32 nblocks = FSSIZE - nmeta;
  
  // 写入超级块
  struct superblock sb;
  sb.magic = 0x10203040;
  sb.size = FSSIZE;
  sb.nblocks = nblocks;
  sb.ninodes = NINODES;
  sb.nlog = LOGSIZE;
  sb.logstart = LOGSTART;
  sb.inodestart = INODESTART;
  sb.bmapstart = BMAPSTART;
  
  // 将超级块写入磁盘
  struct superblock *sb_disk = (struct superblock *)(disk + SUPERBLOCK * BSIZE);
  *sb_disk = sb;
  
  printf("Superblock written:\n");
  printf("  - Total blocks: %d\n", sb.size);
  printf("  - Data blocks: %d\n", sb.nblocks);
  printf("  - Inodes: %d\n", sb.ninodes);
  printf("  - Log blocks: %d\n", sb.nlog);
  
  // 初始化系统
  binit();
  loginit(0, &sb);
  iinit(0);
  
  printf("Creating root directory...\n");
  
  // 创建根目录
  begin_op();
  struct inode *root = ialloc(0, T_DIR);
  if (!root) {
    printf("mkfs: failed to allocate root inode\n");
    return;
  }
  
  ilock(root);
  root->nlink = 1;
  
  // 添加 . 和 .. 目录项
  if (dirlink(root, ".", ROOTINO) < 0) {
    printf("mkfs: failed to create .\n");
  }
  if (dirlink(root, "..", ROOTINO) < 0) {
    printf("mkfs: failed to create ..\n");
  }
  
  iunlock(root);
  iput(root);
  end_op();
  
  printf("Root directory created (inum=%d)\n", ROOTINO);
  printf("Filesystem formatted successfully!\n\n");
}

