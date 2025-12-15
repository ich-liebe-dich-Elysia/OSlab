// 目录操作实现
#include "fs.h"

int printf(const char *fmt, ...);
extern struct inode* iget(uint32 dev, uint32 inum);
extern void ilock(struct inode *ip);
extern void iunlock(struct inode *ip);
extern void iput(struct inode *ip);
extern int readi(struct inode *ip, uint8 *dst, uint32 off, uint32 n);
extern int writei(struct inode *ip, uint8 *src, uint32 off, uint32 n);

// 字符串比较
static int strncmp(const char *s1, const char *s2, int n) {
  for (int i = 0; i < n; i++) {
    if (s1[i] != s2[i]) {
      return s1[i] - s2[i];
    }
    if (s1[i] == '\0') {
      return 0;
    }
  }
  return 0;
}

// 字符串长度
static int strlen(const char *s) {
  int n;
  for (n = 0; s[n]; n++);
  return n;
}

// 字符串复制
static char* strncpy(char *dst, const char *src, int n) {
  int i;
  for (i = 0; i < n && src[i]; i++) {
    dst[i] = src[i];
  }
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return dst;
}

// 在目录中查找文件名
struct inode* dirlookup(struct inode *dp, char *name, uint32 *poff) {
  if (dp->type != T_DIR) {
    printf("dirlookup: not a directory\n");
    return 0;
  }
  
  struct dirent de;
  for (uint32 off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, (uint8*)&de, off, sizeof(de)) != sizeof(de)) {
      printf("dirlookup: readi error\n");
      return 0;
    }
    if (de.inum == 0) {
      continue;
    }
    if (strncmp(name, de.name, DIRSIZ) == 0) {
      if (poff) {
        *poff = off;
      }
      return iget(dp->dev, de.inum);
    }
  }
  
  return 0;
}

// 在目录中添加一个目录项
int dirlink(struct inode *dp, char *name, uint32 inum) {
  // 检查是否已存在
  struct inode *ip;
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }
  
  // 找空闲目录项
  struct dirent de;
  uint32 off;
  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, (uint8*)&de, off, sizeof(de)) != sizeof(de)) {
      printf("dirlink: readi error\n");
      return -1;
    }
    if (de.inum == 0) {
      break;
    }
  }
  
  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, (uint8*)&de, off, sizeof(de)) != sizeof(de)) {
    printf("dirlink: writei error\n");
    return -1;
  }
  
  return 0;
}

// 跳过路径中的斜杠
static char* skipelem(char *path, char *name) {
  while (*path == '/') {
    path++;
  }
  if (*path == 0) {
    return 0;
  }
  
  char *s = path;
  while (*path != '/' && *path != 0) {
    path++;
  }
  int len = path - s;
  if (len >= DIRSIZ) {
    len = DIRSIZ - 1;
  }
  for (int i = 0; i < len; i++) {
    name[i] = s[i];
  }
  name[len] = 0;
  while (*path == '/') {
    path++;
  }
  return path;
}

// 路径名查找
struct inode* namei(char *path) {
  char name[DIRSIZ];
  struct inode *ip, *next;
  
  if (*path == '/') {
    ip = iget(0, ROOTINO);
  } else {
    printf("namei: relative paths not supported\n");
    return 0;
  }
  
  while ((path = skipelem(path, name)) != 0) {
    ilock(ip);
    if (ip->type != T_DIR) {
      iunlock(ip);
      iput(ip);
      return 0;
    }
    if ((next = dirlookup(ip, name, 0)) == 0) {
      iunlock(ip);
      iput(ip);
      return 0;
    }
    iunlock(ip);
    iput(ip);
    ip = next;
  }
  
  return ip;
}

// 查找父目录
struct inode* nameiparent(char *path, char *name) {
  char namebuf[DIRSIZ];
  struct inode *ip, *next;
  
  if (*path == '/') {
    ip = iget(0, ROOTINO);
  } else {
    printf("nameiparent: relative paths not supported\n");
    return 0;
  }
  
  while ((path = skipelem(path, namebuf)) != 0) {
    ilock(ip);
    if (ip->type != T_DIR) {
      iunlock(ip);
      iput(ip);
      return 0;
    }
    if ((next = dirlookup(ip, namebuf, 0)) == 0) {
      iunlock(ip);
      for (int i = 0; i < DIRSIZ; i++) {
        name[i] = namebuf[i];
      }
      return ip;
    }
    iunlock(ip);
    iput(ip);
    ip = next;
  }
  
  iput(ip);
  return 0;
}

