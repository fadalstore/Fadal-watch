.code32
.section .text

.global default_isr
.type default_isr, @function
default_isr:
    cli
1:
    hlt
    jmp 1b
.size default_isr, .-default_isr

.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    movl 4(%esp), %eax
    lgdt (%eax)
    ljmp $0x08, $1f
1:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret
.size gdt_flush, .-gdt_flush

.global keyboard_isr
.type keyboard_isr, @function
keyboard_isr:
    pusha
    call keyboard_interrupt_handler
    movb $0x20, %al
    outb %al, $0x20
    popa
    iret
.size keyboard_isr, .-keyboard_isr

.global timer_isr
.type timer_isr, @function
timer_isr:
    pusha
    call timer_interrupt_handler
    movb $0x20, %al
    outb %al, $0x20
    popa
    iret
.size timer_isr, .-timer_isr

.global syscall_isr
.type syscall_isr, @function
syscall_isr:
    pusha
    call syscall_interrupt_handler
    popa
    iret
.size syscall_isr, .-syscall_isr

.section .note.GNU-stack,"",@progbits
