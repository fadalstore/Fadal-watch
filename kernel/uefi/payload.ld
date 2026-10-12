ENTRY(fadal64_entry)

SECTIONS
{
    . = 0x0000000000000000;
    .fadal_header : ALIGN(8)
    {
        KEEP(*(.fadal_header))
    }
    .text : ALIGN(16)
    {
        KEEP(*(.text.entry))
        *(.text*)
        *(.rodata*)
    }
    .data : ALIGN(16)
    {
        *(.data*)
        *(.sdata*)
        *(.got*)
    }
    .bss (NOLOAD) : ALIGN(16)
    {
        __payload_bss_start = .;
        *(.bss*)
        *(COMMON)
    }
    . = ALIGN(16);
    __payload_mem_end = .;
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
    }
}
