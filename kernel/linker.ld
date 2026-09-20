ENTRY(kernel_main)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R_E */
    rodata PT_LOAD FLAGS(4); /* R__ */
    data PT_LOAD FLAGS(6);   /* RW_ */
}

SECTIONS
{
    . = 0x10000;

    .text : ALIGN(16)
    {
        KEEP(*(.text.entry))
        *(.text*)
    } :text

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    } :rodata

    .data : ALIGN(16)
    {
        *(.data*)
    } :data

    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss*)
    } :data
}
