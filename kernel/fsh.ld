ENTRY(fsh_entry)

SECTIONS
{
    . = 0x00200000;
    .text.entry : { *(.text.entry) }
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
