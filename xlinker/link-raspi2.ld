ENTRY(_kstart)
K_VIR_ADDR = 0x4010000;
K_PHY_ADDR = 0x4010000;
PAGE_SIZE    = 0x100;
/* IRQ/UND/ABT on each core: 512B was too small for timer→perf/schedule. */
STACK_SIZE = 0x1000;
SECTIONS
{
        /* Code. */
        .text K_VIR_ADDR : AT(K_PHY_ADDR)
        {
		. = ALIGN(PAGE_SIZE);
                __start = .;

                *(.text.boot)
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
                *(.data*)
        }
       
        /* Read-write data (uninitialized) and stack */
        .bss BLOCK(4K) : ALIGN(4K)
        {
                 __bss_start = .;
                *(COMMON)
                *(.bss*)
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

                _stack_other = .;
                . = . + STACK_SIZE*4*4 ;

                
                . = ALIGN(4);
                _sstack = .;
        }
        
       __end = .;
}
