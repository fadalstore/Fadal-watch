typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CELLS (VGA_WIDTH * VGA_HEIGHT)
#define IDT_ENTRIES 256
#define TIMER_VECTOR 0x20
#define KEYBOARD_VECTOR 0x21
#define SYSCALL_VECTOR 0x80
#define TIMER_FREQUENCY 100
#define PAGE_SIZE 4096
#define IDENTITY_MAP_BYTES (4 * 1024 * 1024)
#define PHYSICAL_PAGE_COUNT (IDENTITY_MAP_BYTES / PAGE_SIZE)
#define FIRST_USABLE_PAGE (0x100000 / PAGE_SIZE)

static volatile u16 *const vga = (volatile u16 *)0xb8000;
static u32 cursor;
static char command[64];
static u32 command_length;
static u8 shift_pressed;
static volatile u32 timer_ticks;
static volatile u32 syscall_count;
static u32 page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static u8 page_state[PHYSICAL_PAGE_COUNT];
static u32 free_page_count;

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 type_attributes;
    u16 offset_high;
} __attribute__((packed));

struct idt_pointer {
    u16 limit;
    u32 base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];

extern void default_isr(void);
extern void timer_isr(void);
extern void keyboard_isr(void);
extern void syscall_isr(void);

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void lidt(const struct idt_pointer *pointer) {
    __asm__ volatile ("lidtl (%0)" : : "r"(pointer));
}

static inline void enable_interrupts(void) {
    __asm__ volatile ("sti");
}

static inline void halt(void) {
    __asm__ volatile ("hlt");
}

static void kernel_write(const char *text);

static void write_u32(u32 value) {
    char digits[11];
    u32 length = 0;

    if (value == 0) {
        kernel_write("0");
        return;
    }

    while (value > 0 && length < sizeof(digits)) {
        digits[length++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (length > 0) {
        char digit[2] = { digits[--length], '\0' };
        kernel_write(digit);
    }
}

static void paging_init(void) {
    for (u32 index = 0; index < 1024; index++) {
        page_directory[index] = 0;
    }
    /* One 4 MiB identity-mapped page keeps this first memory stage bounded. */
    page_directory[0] = 0x00000083;

    u32 directory = (u32)page_directory;
    __asm__ volatile (
        "mov %%cr4, %%eax\n"
        "orl $0x10, %%eax\n"
        "mov %%eax, %%cr4\n"
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "orl $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(directory)
        : "eax", "memory");
}

static void memory_init(void) {
    for (u32 page = 0; page < PHYSICAL_PAGE_COUNT; page++) {
        page_state[page] = 1;
    }

    free_page_count = 0;
    for (u32 page = FIRST_USABLE_PAGE; page < PHYSICAL_PAGE_COUNT; page++) {
        page_state[page] = 0;
        free_page_count++;
    }
}

static void *page_alloc(void) {
    for (u32 page = FIRST_USABLE_PAGE; page < PHYSICAL_PAGE_COUNT; page++) {
        if (page_state[page] == 0) {
            page_state[page] = 1;
            free_page_count--;

            u32 *memory = (u32 *)(page * PAGE_SIZE);
            for (u32 word = 0; word < PAGE_SIZE / sizeof(u32); word++) {
                memory[word] = 0;
            }
            return memory;
        }
    }
    return (void *)0;
}

static void page_free(void *address) {
    u32 physical_address = (u32)address;
    if ((physical_address % PAGE_SIZE) != 0 ||
        physical_address < 0x100000 ||
        physical_address >= IDENTITY_MAP_BYTES) {
        return;
    }

    u32 page = physical_address / PAGE_SIZE;
    if (page_state[page] != 0) {
        page_state[page] = 0;
        free_page_count++;
    }
}

static void idt_set_gate(u8 vector, void (*handler)(void), u8 attributes) {
    u32 address = (u32)handler;
    idt[vector].offset_low = (u16)(address & 0xffff);
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].type_attributes = attributes;
    idt[vector].offset_high = (u16)(address >> 16);
}

static void idt_init(void) {
    for (u32 vector = 0; vector < IDT_ENTRIES; vector++) {
        idt_set_gate((u8)vector, default_isr, 0x8e);
    }
    idt_set_gate(TIMER_VECTOR, timer_isr, 0x8e);
    idt_set_gate(KEYBOARD_VECTOR, keyboard_isr, 0x8e);
    /* DPL 3 makes the ABI callable by a future user process. */
    idt_set_gate(SYSCALL_VECTOR, syscall_isr, 0xee);

    struct idt_pointer pointer = {
        .limit = (u16)(sizeof(idt) - 1),
        .base = (u32)idt,
    };
    lidt(&pointer);
}

static void pic_init(void) {
    outb(0x20, 0x11);
    io_wait();
    outb(0xa0, 0x11);
    io_wait();

    outb(0x21, 0x20);
    io_wait();
    outb(0xa1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();
    outb(0xa1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();
    outb(0xa1, 0x01);
    io_wait();

    /* Keep only timer and keyboard IRQs unmasked during this milestone. */
    outb(0x21, 0xfc);
    outb(0xa1, 0xff);
}

static void timer_init(void) {
    const u16 divisor = (u16)(1193182 / TIMER_FREQUENCY);
    outb(0x43, 0x36);
    outb(0x40, (u8)(divisor & 0xff));
    outb(0x40, (u8)(divisor >> 8));
}

static void serial_init(void) {
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x80);
    outb(0x3f8 + 0, 0x03);
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x03);
    outb(0x3f8 + 2, 0xc7);
    outb(0x3f8 + 4, 0x0b);
}

static void serial_putc(char value) {
    while ((inb(0x3f8 + 5) & 0x20) == 0) {
    }
    outb(0x3f8, (u8)value);
}

static void serial_write(const char *text) {
    while (*text != '\0') {
        serial_putc(*text++);
    }
}

static void vga_clear(void) {
    for (u32 index = 0; index < VGA_CELLS; index++) {
        vga[index] = (u16)(' ' | (0x1f << 8));
    }
    cursor = 0;
}

static void vga_scroll(void) {
    if (cursor < VGA_CELLS) {
        return;
    }

    for (u32 row = 1; row < VGA_HEIGHT; row++) {
        for (u32 column = 0; column < VGA_WIDTH; column++) {
            vga[(row - 1) * VGA_WIDTH + column] =
                vga[row * VGA_WIDTH + column];
        }
    }

    for (u32 column = 0; column < VGA_WIDTH; column++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + column] = (u16)(' ' | (0x1f << 8));
    }
    cursor = (VGA_HEIGHT - 1) * VGA_WIDTH;
}

static void vga_putc(char value) {
    if (value == '\r') {
        return;
    }

    if (value == '\n') {
        cursor = (cursor / VGA_WIDTH + 1) * VGA_WIDTH;
        vga_scroll();
        return;
    }

    if (value == '\b') {
        if (cursor > 0) {
            cursor--;
            vga[cursor] = (u16)(' ' | (0x1f << 8));
        }
        return;
    }

    vga[cursor++] = (u16)(value | (0x1f << 8));
    vga_scroll();
}

static void vga_write(const char *text) {
    while (*text != '\0') {
        vga_putc(*text++);
    }
}

static void kernel_write(const char *text) {
    vga_write(text);
    serial_write(text);
}

static char keyboard_ascii(u8 scancode) {
    switch (scancode) {
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0a: return '9';
        case 0x0b: return '0';
        case 0x0c: return '-';
        case 0x0d: return '=';
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1a: return '[';
        case 0x1b: return ']';
        case 0x1e: return 'a';
        case 0x1f: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x27: return ';';
        case 0x28: return '\'';
        case 0x29: return '`';
        case 0x2b: return '\\';
        case 0x2c: return 'z';
        case 0x2d: return 'x';
        case 0x2e: return 'c';
        case 0x2f: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';
        case 0x39: return ' ';
        default: return '\0';
    }
}

static u8 text_equal(const char *left, const char *right) {
    u32 index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }
        index++;
    }
    return left[index] == right[index];
}

static void print_memory_stats(void) {
    kernel_write("memory: ");
    write_u32(free_page_count);
    kernel_write(" / ");
    write_u32(PHYSICAL_PAGE_COUNT - FIRST_USABLE_PAGE);
    kernel_write(" usable pages free\n");
}

static void command_reset(void) {
    command_length = 0;
    command[0] = '\0';
}

static void command_run(void) {
    kernel_write("\n");

    if (command_length == 0) {
        /* Empty input just redraws the prompt. */
    } else if (text_equal(command, "help")) {
        kernel_write("commands: help info mem uptime status clear\n");
    } else if (text_equal(command, "info")) {
        kernel_write("fadal kernel: 32-bit protected mode\n");
        kernel_write("console: VGA text + PS/2 IRQ1\n");
        kernel_write("memory: 4 MiB identity map + page allocator\n");
        kernel_write("timer: PIT IRQ0 at 100 Hz\n");
        kernel_write("syscalls: int 0x80 ABI gate online\n");
    } else if (text_equal(command, "mem")) {
        print_memory_stats();
    } else if (text_equal(command, "uptime")) {
        kernel_write("uptime: ");
        write_u32(timer_ticks / TIMER_FREQUENCY);
        kernel_write("s (");
        write_u32(timer_ticks);
        kernel_write(" ticks)\n");
    } else if (text_equal(command, "status")) {
        kernel_write("status: kernel online; IRQ0 + IRQ1 active\n");
        kernel_write("status: syscall count ");
        write_u32(syscall_count);
        kernel_write("\n");
        print_memory_stats();
    } else if (text_equal(command, "clear")) {
        vga_clear();
    } else {
        kernel_write("unknown command; try help\n");
    }

    command_reset();
    kernel_write("> ");
}

static void keyboard_handle(u8 scancode) {
    if (scancode == 0x2a || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xaa || scancode == 0xb6) {
        shift_pressed = 0;
        return;
    }
    if ((scancode & 0x80) != 0) {
        return;
    }
    if (scancode == 0x1c) {
        command_run();
        return;
    }
    if (scancode == 0x0e) {
        if (command_length > 0) {
            command[--command_length] = '\0';
            kernel_write("\b");
        }
        return;
    }

    char value = keyboard_ascii(scancode);
    if (value == '\0' || command_length >= sizeof(command) - 1) {
        return;
    }
    if (shift_pressed && value >= 'a' && value <= 'z') {
        value = (char)(value - 'a' + 'A');
    }
    command[command_length++] = value;
    command[command_length] = '\0';
    vga_putc(value);
    serial_putc(value);
}

void keyboard_interrupt_handler(void) {
    keyboard_handle(inb(0x60));
}

void timer_interrupt_handler(void) {
    timer_ticks++;
}

void syscall_interrupt_handler(void) {
    syscall_count++;
}

__attribute__((section(".text.entry"), used))
void kernel_main(void) {
    serial_init();
    vga_clear();

    kernel_write("FADAL KERNEL ONLINE\n");
    kernel_write("-------------------\n");
    kernel_write("mode: 32-bit protected mode\n");
    kernel_write("origin: from-scratch, no Linux dependency\n");
    kernel_write("status: boot path verified\n");
    memory_init();
    paging_init();
    kernel_write("memory: 4 MiB identity paging online\n");
    void *test_page = page_alloc();
    if (test_page != (void *)0) {
        page_free(test_page);
        kernel_write("memory: page allocation self-test passed\n");
    } else {
        kernel_write("memory: page allocation self-test failed\n");
    }
    idt_init();
    pic_init();
    timer_init();
    kernel_write("interrupts: IDT + PIC online\n");
    kernel_write("timer: PIT IRQ0 online at 100 Hz\n");
    kernel_write("syscalls: int 0x80 ABI gate online\n");
    kernel_write("\nFadal console ready. Type help.\n> ");
    enable_interrupts();

    for (;;) {
        halt();
    }
}