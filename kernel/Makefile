# 最小操作系统 Makefile
# 构建参考xv6的简化版RISC-V操作系统

# RISC-V交叉编译工具链
CROSS_COMPILE = riscv64-unknown-elf-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump

# 编译选项（参考xv6配置）
CFLAGS = -Wall -Werror -O2 -fno-omit-frame-pointer -ggdb
CFLAGS += -mcmodel=medany -ffreestanding -fno-common -nostdlib
CFLAGS += -mno-relax -fno-stack-protector -fno-pie -no-pie

# 链接选项
LDFLAGS = -z max-page-size=4096

# 源文件
OBJS = entry.o main.o uart.o printf.o console.o
TARGET = kernel

# === 构建目标 ===
all: $(TARGET)
	@echo "=== Hello OS Kernel Built Successfully ==="
	@echo "Use 'make qemu' to run in QEMU"

$(TARGET): $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o $(TARGET) $(OBJS)
	$(OBJDUMP) -S $(TARGET) > $(TARGET).asm

# 编译规则
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# === 运行目标 ===
qemu: $(TARGET)
	@echo "=== Starting Hello OS in QEMU ==="
	@echo "Expected output: Hello OS!"
	@echo "Press Ctrl+C to exit QEMU"
	@echo "========================================="
	qemu-system-riscv64 -machine virt -bios none -kernel $(TARGET) -m 128M -nographic -serial mon:stdio

# 调试模式
qemu-gdb: $(TARGET)
	@echo "=== Starting Hello OS with GDB support ==="
	@echo "Connect GDB with: gdb $(TARGET), then (gdb) target remote :1234"
	qemu-system-riscv64 -machine virt -bios none -kernel $(TARGET) -m 128M -nographic -s -S

# === 清理目标 ===
clean:
	rm -f *.o $(TARGET) $(TARGET).asm
	@echo "Build files cleaned"

# === 帮助信息 ===
help:
	@echo "Hello OS Build System"
	@echo "===================="
	@echo "Targets:"
	@echo "  all      - Build the Hello OS kernel"
	@echo "  qemu     - Run Hello OS in QEMU (shows 'Hello OS!' output)"
	@echo "  qemu-alt - Alternative QEMU command (if serial issues)"
	@echo "  qemu-gdb - Run with GDB support for debugging"
	@echo "  clean    - Clean build files"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - riscv64-unknown-elf-gcc toolchain"
	@echo "  - qemu-system-riscv64"

.PHONY: all qemu qemu-alt qemu-gdb clean help
