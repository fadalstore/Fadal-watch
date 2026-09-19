.code16
.section .text
.global _start

.equ KERNEL_SECTORS, 48
.equ BOOT_BASE, 0x7c00

_start:
    cli
    xorw %ax, %ax
    movw %ax, %ss
    movw $BOOT_BASE, %sp
    movw $0x07c0, %ax
    movw %ax, %ds
    movb %dl, boot_drive

    # Read the fixed kernel budget from sector 2 into physical address 0x10000.
    movw $0x1000, %ax
    movw %ax, %es
    xorw %bx, %bx
    movb $0x02, %ah
    movb $KERNEL_SECTORS, %al
    movb $0x00, %ch
    movb $0x02, %cl
    movb $0x00, %dh
    movb boot_drive, %dl
    int $0x13
    jc disk_error

    lgdt gdt_descriptor
    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0
    ljmp $0x08, $(protected_mode + BOOT_BASE)

disk_error:
    cli
1:
    hlt
    jmp 1b

.code32
protected_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $0x90000, %esp
    movl $0x10000, %eax
    call *%eax
2:
    hlt
    jmp 2b

.align 8
gdt:
    .quad 0x0000000000000000
    # Flat 32-bit code segment.
    .quad 0x00cf9a000000ffff
    # Flat 32-bit data segment.
    .quad 0x00cf92000000ffff
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt - 1
    .long gdt + BOOT_BASE

boot_drive:
    .byte 0

.org 510
.word 0xaa55
