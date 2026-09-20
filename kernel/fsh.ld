ENTRY(fsh_entry)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R_E */
    rodata PT_LOAD FLAGS(4); /* R__ */
    data PT_LOAD FLAGS(6);   /* RW_ */
}

SECTIONS
{
    . = 0x00200000;
    .text.entry : { *(.text.entry) } :text
    .text : { *(.text*) } :text
    .rodata : { *(.rodata*) } :rodata
    .data : { *(.data*) } :data
    .bss : { *(.bss*) *(COMMON) } :data
}
