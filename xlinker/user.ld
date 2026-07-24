ENTRY(_start)
K_VIR_ADDR = 0x40000000;
K_PHY_ADDR = 0x40000000;
PAGE_SIZE    = 0x1000;
SECTIONS
{
        /* Code. */
        .text K_VIR_ADDR : AT(K_PHY_ADDR)
        {
		. = ALIGN(PAGE_SIZE);
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

                PROVIDE_HIDDEN (__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
                KEEP(*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);

                PROVIDE (__preinit_array_start = .);
	        KEEP (*(.preinit_array))
	        PROVIDE (__preinit_array_end = .);

                PROVIDE (__fini_array_start = .);
		KEEP (*(.fini_array))
		KEEP (*(SORT(.fini_array.*)))
		PROVIDE (__fini_array_end = .);
        }
        /* Read-write data (uninitialized) and stack */
        .bss BLOCK(4K) : ALIGN(4K)
        {
                *(COMMON)
                *(.bss*)
        }
        .eh_frame BLOCK(4K) : ALIGN(4K)
        {
                *(.eh_frame)
        }
        /* Thread Local Storage sections  */
        .tdata          : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
        .tbss           : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
       _end = .;
}
