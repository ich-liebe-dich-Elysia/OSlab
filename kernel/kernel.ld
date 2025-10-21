/* 
 * 最小操作系统链接脚本
 */

OUTPUT_ARCH("riscv")
ENTRY(_entry)

SECTIONS
{
    /* 内核加载地址
     * QEMU -kernel 将内核加载到此地址
     * 这是RISC-V标准的内核起始地址
     */
    . = 0x80000000;

    /* 代码段 
     * 包含所有可执行代码
     */
    .text : {
        /* 确保启动代码entry.S位于最前面 */
        */entry.o(.text)
        *(.text .text.*)
        PROVIDE(etext = .);
    }

    /* 只读数据段 
     * 包含字符串常量等只读数据
     */
    .rodata : {
        . = ALIGN(16);
        *(.rodata .rodata.*)
    }

    /* 初始化数据段 
     * 包含有初值的全局变量
     */
    .data : {
        . = ALIGN(16);
        *(.data .data.*)
        PROVIDE(edata = .);
    }

    /* BSS段
     * 包含未初始化的全局变量和栈空间
     * entry.S会将这个区域清零
     */
    .bss : {
        . = ALIGN(16);
        PROVIDE(__bss_start = .);
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        PROVIDE(__bss_end = .);
    }

    /* 内核结束标记 
     * 标记内核映像的结束位置
     */
    PROVIDE(end = .);
}
