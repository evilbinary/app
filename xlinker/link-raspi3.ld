ENTRY(_start)
K_VIR_ADDR = 0x100000;
K_PHY_ADDR = 0x100000;
PAGE_SIZE    = 0x100;
STACK_SIZE = 0x2000;

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
                _sidata = .;  /* Load addr = virtual addr for QEMU direct load */
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
