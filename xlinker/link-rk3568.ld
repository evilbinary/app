ENTRY(_start)
/* RK3568 Z96A：U-Boot kernel_addr_r = 0x02080000。
 * 与 boot/arm64/init.h 的 KERNEL_BASE 以及 uImage 的 -a/-e 相同。 */
K_VIR_ADDR = 0x02080000;
K_PHY_ADDR = 0x02080000;
PAGE_SIZE    = 0x100;
STACK_SIZE = 0x8000;
__stack_size = STACK_SIZE;

SECTIONS
{
        .text K_VIR_ADDR : AT(K_PHY_ADDR)
        {
		. = ALIGN(PAGE_SIZE);
                __start = .;
                KEEP(*(.text.boot))
                *(.text*)
        }

        .rodata BLOCK(4K) : ALIGN(4K)
        {
                *(.rodata*)
        }

        .data BLOCK(4K) : ALIGN(4K)
        {
                _sdata = .;
                _sidata = .;
                *(.data*)
                _edata = .;
        }

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
