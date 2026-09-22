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
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
    }
}
