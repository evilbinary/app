ENTRY(_start)
/* Raspberry Pi 5: firmware loads raw kernel image to 0x80000 */
K_VIR_ADDR = 0x80000;
K_PHY_ADDR = 0x80000;
PAGE_SIZE    = 0x100;
/* 【CPU0 可用栈 = .bss 之后预留的这个空隙】boot asm 用 sp = _estack，栈向下生长：
 *   _estack(0x140000) - __bss_end(0x13ddd4) = 只有 8748 字节 ✗
 * 内核早期（boot 阶段帧 + kprintf/kvsnprintf + 异常路径）实测会踩进 .bss，
 * 把 printf_buffer/print_lock 与活跃栈互相覆盖 ⇒ 静默死（无 fault、无输出）。
 * 0x2000 → 0x8000：空隙变 32KB。注意 STACK_SIZE 同时用于 6 个异常栈和
 * _stack_other(16×)，全部一起变大（RAM 足够）。 */
STACK_SIZE = 0x8000;
__stack_size = STACK_SIZE;

SECTIONS
{
        /* Code. */
        .text K_VIR_ADDR : AT(K_PHY_ADDR)
        {
		. = ALIGN(PAGE_SIZE);
                __start = .;
                KEEP(*(.text.boot))    /* Boot code must be first */
                *(.text*)
        }

        /* Read-only data. */
        .rodata BLOCK(4K) : ALIGN(4K)
        {
                *(.rodata*)
        }

        /* Read-write data (initialized) */
        .data BLOCK(4K) : ALIGN(4K)
        {
                _sdata = .;
                _sidata = .;  /* Load addr = virtual addr for firmware direct load */
                *(.data*)
                _edata = .;
        }

        /* Read-write data (uninitialized) and stack */
        .bss BLOCK(4K) : ALIGN(4K)
        {
                __bss_start = .;
                *(COMMON)
                *(.bss*)
                __bss_end = .;
        }

        . = ALIGN(4K) + STACK_SIZE;
        .stack : ALIGN(4K) {
                _estack = .;
                . = . + STACK_SIZE;
                _stack_abt = .;

                . = . + STACK_SIZE;
                _stack_undf = .;

                . = . + STACK_SIZE;
                _stack_irq = .;

                . = . + STACK_SIZE;
                _stack_svc = .;

                . = . + STACK_SIZE;
                _stack_sys = .;

                _stack_other = .;
                . = . + STACK_SIZE*4*4;

                . = ALIGN(4);
                _sstack = .;
        }

        __end = .;
}
