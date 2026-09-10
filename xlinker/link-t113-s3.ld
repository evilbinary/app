ENTRY(_kstart)
K_VIR_ADDR = 0x42000000;
K_PHY_ADDR = 0x42000000;
PAGE_SIZE    = 0x100;
STACK_SIZE = 0x1000;
__stack_size = STACK_SIZE;
SECTIONS
{
        /* Code. */
        .text K_VIR_ADDR : AT(K_PHY_ADDR)
        {
		. = ALIGN(PAGE_SIZE);
                __start = .;
                KEEP(*(.text.boot .text.boot.*)) 
                *(.text .text.*)
        }

        /* Read-only data. */
        .rodata BLOCK(4K) : ALIGN(4K)
        {
                *(.rodata*)
        }

        /* Read-write data (initialized) */
        .data BLOCK(4K) : ALIGN(4K)
        {
                *(.data*)
        }

        /* Read-write data (uninitialized) and stack */
        
        .bss BLOCK(4K) : ALIGN(4K)
        {
                __bss_start = .;
                *(COMMON)
                *(.bss .bss*)
                __bss_end = .;
        }
        
        .stack :  ALIGN(4K) {
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

                /* _stack_other 必须是这块 APU 栈区的【基址】：apu_entry 里按
                 * base + cpu*(4*STACK_SIZE) 逐块取用。写在预留之后（=区域末尾）
                 * 会让其它核的四种模式栈整体偏出一个区域，落到保留区之外，
                 * 压栈即改写内核数据 —— 真机挂、QEMU 不挂的差别就在这里。 */
                _stack_other = .;
                . = . + STACK_SIZE*4*4 ;

                
                . = ALIGN(4);
                _sstack = .;
        }
        
       __end = .;
       _end = .;
}
