// 简化文件系统定义
#ifndef FS_H
#define FS_H

typedef unsigned long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

// 文件系统布局常量
#define BSIZE 512                // 块大小
#define FSSIZE 1000              // 文件系统大小（块数）
#define ROOTINO 1                // 根目录 inode 号
#define NDIRECT 11               // 直接块数量
#define NINDIRECT (BSIZE / sizeof(uint32))  // 间接块数量
#define MAXFILE (NDIRECT + NINDIRECT)       // 最大文件大小（块数）

// 磁盘布局
#define SUPERBLOCK 1             // 超级块位置
#define LOGSTART 2               // 日志起始块
#define LOGSIZE 30               // 日志大小
#define INODESTART (LOGSTART + LOGSIZE)  // inode区起始
#define NINODES 200              // inode数量
#define NBITMAP 1                // 位图块数
#define BMAPSTART (INODESTART + NINODES/(512/sizeof(struct dinode)))
#define DATASTART (BMAPSTART + NBITMAP)

// 文件类型
#define T_DIR  1   // 目录
#define T_FILE 2   // 文件
#define T_DEV  3   // 设备

// 超级块结构
struct superblock {
  uint32 magic;        // 魔数 0x10203040
  uint32 size;         // 文件系统大小（块数）
  uint32 nblocks;      // 数据块数量
  uint32 ninodes;      // inode数量
  uint32 nlog;         // 日志块数量
  uint32 logstart;     // 日志起始块号
  uint32 inodestart;   // inode区起始块号
  uint32 bmapstart;    // 位图起始块号
};

// 磁盘上的 inode 结构
struct dinode {
  uint16 type;         // 文件类型
  uint16 major;        // 主设备号（设备文件）
  uint16 minor;        // 次设备号（设备文件）
  uint16 nlink;        // 硬链接计数
  uint32 size;         // 文件大小（字节）
  uint32 addrs[NDIRECT+1];  // 数据块地址（最后一个是间接块）
};

// 内存中的 inode 结构
struct inode {
  uint32 dev;          // 设备号
  uint32 inum;         // inode号
  int ref;             // 引用计数
  int valid;           // 是否已从磁盘加载
  
  // 从磁盘拷贝的字段
  uint16 type;
  uint16 major;
  uint16 minor;
  uint16 nlink;
  uint32 size;
  uint32 addrs[NDIRECT+1];
};

// 目录项结构
#define DIRSIZ 14
struct dirent {
  uint16 inum;         // inode号（0表示空闲）
  char name[DIRSIZ];   // 文件名
};

// inode 磁盘块中的 inode 数量
#define IPB (BSIZE / sizeof(struct dinode))

// inode 号对应的块号
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// 位图相关宏
#define BPB (BSIZE*8)  // 每个位图块的位数
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

#endif

