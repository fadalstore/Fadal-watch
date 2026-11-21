ENTRY(fsh_entry)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R_E */
    rodata PT_LOAD FLAGS(4); /* R__ */
    data PT_LOAD FLAGS(6);   /* RW_ */
    probe PT_LOAD FLAGS(5);  /* R_E, fixed one-page negative-control thunk */
}

SECTIONS
{
    . = 0x00200000;
    .text.entry : { *(.text.entry) } :text
    .text : { *(.text*) } :text
    .rodata : { *(.rodata*) } :rodata
    ASSERT(. <= 0x00200f00, "FSH text/rodata overlaps the fixed probe slot")
    .fsh_probe 0x00200f00 : { KEEP(*(.fsh_probe)) } :probe
    .data : { *(.data*) } :data
    .bss : { *(.bss*) *(COMMON) } :data
    ASSERT(ADDR(.fsh_probe) == 0x00200f00, "FSH probe slot moved")
    ASSERT(ADDR(.bss) + SIZEOF(.bss) <= 0x00201000,
           "FSH exceeds its one-page user-code mapping")
}
