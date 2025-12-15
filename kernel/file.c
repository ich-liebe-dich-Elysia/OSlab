// 文件操作实现
#include "fs.h"

int printf(const char *fmt, ...);
extern struct inode* ialloc(uint32 dev, uint16 type);
extern struct inode* iget(uint32 dev, uint32 inum);
extern void ilock(struct inode *ip);
extern void iunlock(struct inode *ip);
extern void iput(struct inode *ip);
extern void iupdate(struct inode *ip);
extern void itrunc(struct inode *ip);
extern int readi(struct inode *ip, uint8 *dst, uint32 off, uint32 n);
extern int writei(struct inode *ip, uint8 *src, uint32 off, uint32 n);
extern struct inode* namei(char *path);
extern struct inode* nameiparent(char *path, char *name);
extern int dirlink(struct inode *dp, char *name, uint32 inum);
extern void begin_op(void);
extern void end_op(void);

// 打开文件表
struct file {
  int ref;         // 引用计数
  char readable;
  char writable;
  struct inode *ip;
  uint32 off;      // 当前偏移
};

#define NFILE 100
static struct file ftable[NFILE];

// 分配一个文件结构
struct file* filealloc(void) {
  for (int i = 0; i < NFILE; i++) {
    if (ftable[i].ref == 0) {
      ftable[i].ref = 1;
      return &ftable[i];
    }
  }
  return 0;
}

// 增加文件引用计数
struct file* filedup(struct file *f) {
  if (f->ref < 1) {
    printf("filedup: invalid file\n");
    return 0;
  }
  f->ref++;
  return f;
}

// 关闭文件
void fileclose(struct file *f) {
  if (f->ref < 1) {
    printf("fileclose: invalid file\n");
    return;
  }
  
  f->ref--;
  if (f->ref == 0) {
    struct inode *ip = f->ip;
    if (ip) {
      begin_op();
      iput(ip);
      end_op();
    }
  }
}

// 读取文件
int fileread(struct file *f, uint8 *addr, int n) {
  if (!f->readable) {
    return -1;
  }
  
  ilock(f->ip);
  int r = readi(f->ip, addr, f->off, n);
  if (r > 0) {
    f->off += r;
  }
  iunlock(f->ip);
  
  return r;
}

// 写入文件
int filewrite(struct file *f, uint8 *addr, int n) {
  if (!f->writable) {
    return -1;
  }
  
  ilock(f->ip);
  int r = writei(f->ip, addr, f->off, n);
  if (r > 0) {
    f->off += r;
  }
  iunlock(f->ip);
  
  return r;
}

// 创建文件
struct inode* create(char *path, uint16 type) {
  char name[DIRSIZ];
  struct inode *dp = nameiparent(path, name);
  if (dp == 0) {
    return 0;
  }
  
  ilock(dp);
  
  // 检查是否已存在
  struct inode *ip;
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iunlock(dp);
    iput(dp);
    ilock(ip);
    if (type == T_FILE && ip->type == T_FILE) {
      return ip;  // 文件已存在
    }
    iunlock(ip);
    iput(ip);
    return 0;
  }
  
  // 分配新inode
  if ((ip = ialloc(dp->dev, type)) == 0) {
    iunlock(dp);
    iput(dp);
    return 0;
  }
  
  ilock(ip);
  ip->nlink = 1;
  iupdate(ip);
  
  // 如果是目录，添加 . 和 ..
  if (type == T_DIR) {
    dp->nlink++;  // 父目录链接数增加
    iupdate(dp);
    
    // . 指向自己
    if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0) {
      printf("create: dirlink error\n");
      goto fail;
    }
  }
  
  // 在父目录中添加目录项
  if (dirlink(dp, name, ip->inum) < 0) {
    printf("create: dirlink error\n");
    goto fail;
  }
  
  iunlock(dp);
  iput(dp);
  
  return ip;
  
fail:
  ip->nlink = 0;
  iupdate(ip);
  iunlock(ip);
  iput(ip);
  iunlock(dp);
  iput(dp);
  return 0;
}

