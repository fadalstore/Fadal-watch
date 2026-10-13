ENTRY(fadal64_entry)

PHDRS
{
    metadata PT_LOAD FLAGS(4);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x0000000000000000;
    .fadal_header : ALIGN(8)
    {
        KEEP(*(.fadal_header))
    } :metadata
    .fadal_layout : ALIGN(8)
    {
        KEEP(*(.fadal_layout))
    } :metadata

    . = ALIGN(4096);
    __payload_text_start = .;
    .text :
    {
        KEEP(*(.text.entry))
        *(.text*)
        . = ALIGN(4096);
    } :text
    __payload_text_end = .;

    __payload_rodata_start = .;
    .rodata :
    {
        *(.rodata*)
        . = ALIGN(4096);
    } :rodata
    __payload_rodata_end = .;

    __payload_data_start = .;
    .data :
    {
        *(.data*)
        *(.sdata*)
        *(.got*)
        . = ALIGN(4096);
    } :data
    __payload_data_end = .;

    __payload_bss_start = .;
    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
        . = ALIGN(4096);
    } :data
    __payload_mem_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
    }
}
