ENTRY(kernel_main)

SECTIONS
{
    . = 0x10000;

    .text : ALIGN(16)
    {
        KEEP(*(.text.entry))
        *(.text*)
    }

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    }

    .data : ALIGN(16)
    {
        *(.data*)
    }

    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss*)
    }
}