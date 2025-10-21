# Hello OS - 最小RISC-V操作系统

通过参考xv6的启动机制，理解并实现最小操作系统的引导过程，最终在QEMU中输出**"Hello OS"**。

## 实验目标

🎯 **主要目标**：理解操作系统启动流程
- 从汇编启动代码到C语言环境的转换
- BSS段清零的重要性
- 栈设置和C运行时环境准备
- 基本硬件初始化（UART串口）

🎯 **预期输出**：在QEMU中看到 **"HELLO OS!"** 的成功启动信息

## 核心特性

✅ **完整启动流程**：参考xv6的entry.S实现  
✅ **BSS段清零**：确保未初始化变量正确为0  
✅ **UART串口驱动**：实现基本的文本输出  
✅ **内存布局管理**：通过链接脚本控制内存组织  
✅ **单核简化设计**：专注理解核心概念

## 文件结构

```
kernel/
├── entry.S      # 启动汇编代码
├── kernel.ld    # 链接脚本
├── main.c       # 主函数
├── uart.c       # UART驱动
├── Makefile     # 编译脚本
└── README.md    # 说明文档
```

## 编译要求

需要安装RISC-V交叉编译工具链：

```bash
# Ubuntu/Debian
sudo apt-get install gcc-riscv64-unknown-elf

# 或者下载预编译工具链
# https://github.com/riscv/riscv-gnu-toolchain
```

## 编译和运行

```bash
# 进入kernel目录
cd kernel

# 编译内核
make

# 运行QEMU（需要安装qemu-system-riscv64）
make qemu

# 调试模式运行
make qemu-gdb
```

## 预期输出

运行 `make qemu` 后，你将看到：

```
==========================================
        Hello OS - RISC-V Kernel         
==========================================
Boot process completed successfully!

BSS segment verification: PASS (uninitialized global = 0)

Minimal OS features initialized:
  [x] Stack setup (from entry.S)
  [x] BSS segment cleared
  [x] UART serial output
  [x] C runtime environment

*****************************************
*              HELLO OS!               *
*    Minimal RISC-V OS is running!    *
*****************************************

System is now running and ready.
Entering idle loop...
```

🎉 这表明你已经成功实现了一个最小但完整的操作系统启动过程！

## 关键学习点

通过这个Hello OS实验，你将深入理解：

### 1. **启动流程**（参考xv6/entry.S）
- 硬件加载内核到内存的过程
- 汇编代码如何准备C语言运行环境
- 栈指针设置的重要性

### 2. **内存管理基础**
- 链接脚本如何组织内存布局
- BSS段清零确保程序正确性
- 代码段、数据段、BSS段的区别

### 3. **硬件接口**
- UART串口寄存器的基本操作
- 内存映射I/O的概念
- 硬件初始化的基本步骤

## 与xv6的区别

这个实现相比xv6进行了大幅简化：

### 移除的功能
- 多核支持
- 虚拟内存管理
- 进程调度
- 系统调用
- 中断处理
- 复杂的设备驱动
- 文件系统

### 保留的核心功能
- 基本启动流程
- 栈设置
- BSS段清零
- UART串口输出
- 链接脚本和内存布局

## 学习价值

这个最小内核帮助理解：

1. **启动流程**：从汇编到C的完整过程
2. **内存管理**：链接脚本和内存段的概念
3. **硬件接口**：UART寄存器操作
4. **调试技术**：启动阶段的调试方法
5. **系统设计**：最小功能集的选择

## 扩展建议

可以在此基础上添加：

1. **简单内存分配器**
2. **基本中断处理**
3. **定时器支持**
4. **简单的用户程序加载**
5. **基本的错误处理机制**

## 故障排除

### 编译错误
- 检查RISC-V工具链是否正确安装
- 确认工具链前缀是否为 `riscv64-unknown-elf-`

### 运行问题
- 确认QEMU版本支持RISC-V
- 检查是否安装了 `qemu-system-riscv64`

### 无输出
- 确认QEMU串口配置正确
- 检查UART初始化代码

## 参考资料

- [xv6 RISC-V](https://github.com/mit-pdos/xv6-riscv)
- [RISC-V Instruction Set Manual](https://riscv.org/specifications/)
- [UART 16550 Datasheet](http://byterunner.com/16550.html)
