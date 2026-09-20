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
# C receives the pusha frame and returns the frame pointer to restore.
timer_isr:
    pusha
    pushl %esp
    call timer_interrupt_handler
    addl $4, %esp
    movl %eax, %esp
    movb $0x20, %al
    outb %al, $0x20
    popa
    iret
.size timer_isr, .-timer_isr

.global syscall_isr
.type syscall_isr, @function
syscall_isr:
    pusha
    pushl %esp
    call syscall_interrupt_handler
    addl $4, %esp
    popa
    iret
.size syscall_isr, .-syscall_isr

.global page_fault_isr
.type page_fault_isr, @function
page_fault_isr:
    cli
    pusha
    pushl %esp
    call page_fault_interrupt_handler
    addl $4, %esp
    popa
    addl $4, %esp
    iret
.size page_fault_isr, .-page_fault_isr

.section .note.GNU-stack,"",@progbits
