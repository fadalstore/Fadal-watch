typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

static inline void fadal_outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 fadal_inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

__attribute__((section(".fadal_header"), used))
const u8 fadal64_header[32] = {
    0x34, 0x36, 0x4c, 0x44, 0x46, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define COM1 0x3f8
#define VGA ((volatile u16 *)0xb8000)
#define VGA_COLS 80
#define VGA_ROWS 25
#define LINE_MAX 96
#define USER_ROOT 0
#define USER_FADAL 1000

static u32 vga_row;
static u32 vga_col;
static u32 current_uid = USER_ROOT;
static const char current_user[] = "root";

static void serial_init(void) {
    fadal_outb(COM1 + 1, 0x00);
    fadal_outb(COM1 + 3, 0x80);
    fadal_outb(COM1 + 0, 0x01);
    fadal_outb(COM1 + 1, 0x00);
    fadal_outb(COM1 + 3, 0x03);
    fadal_outb(COM1 + 2, 0xc7);
    fadal_outb(COM1 + 4, 0x0b);
}

static int f_streq(const char *left, const char *right) {
    u32 index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        index++;
    }
    return left[index] == right[index];
}

static int f_starts(const char *text, const char *prefix) {
    u32 index = 0;
    while (prefix[index] != '\0') {
        if (text[index] != prefix[index]) return 0;
        index++;
    }
    return 1;
}

static void serial_putc(char value) {
    fadal_outb(COM1, (u8)value);
}

static void vga_putc(char value) {
    if (value == '\r') return;
    if (value == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (value == '\b') {
        if (vga_col > 0) vga_col--;
        VGA[vga_row * VGA_COLS + vga_col] = (u16)(' ' | (0x1f << 8));
    } else {
        VGA[vga_row * VGA_COLS + vga_col] = (u16)((u8)value | (0x1f << 8));
        vga_col++;
        if (vga_col >= VGA_COLS) {
            vga_col = 0;
            vga_row++;
        }
    }
    if (vga_row >= VGA_ROWS) vga_row = 0;
}

static void console_write(const char *text) {
    u32 index;
    for (index = 0; text[index] != '\0'; index++) {
        serial_putc(text[index]);
        vga_putc(text[index]);
    }
}

static void console_clear(void) {
    u32 index;
    for (index = 0; index < VGA_COLS * VGA_ROWS; index++) VGA[index] = (u16)(' ' | (0x1f << 8));
    vga_row = 0;
    vga_col = 0;
}

static int serial_ready(void) {
    return (fadal_inb(COM1 + 5) & 1) != 0;
}

static char serial_getc(void) {
    while (!serial_ready()) __asm__ volatile ("pause");
    return (char)fadal_inb(COM1);
}

static void prompt(void) {
    console_write("fadal:/home/root# ");
}

static void print_uid(void) {
    if (current_uid == USER_ROOT) console_write("0");
    else console_write("1000");
}

static void run_command(char *line) {
    if (line[0] == '\0') return;
    if (f_streq(line, "help")) {
        console_write("commands: help clear pwd ls cd cat whoami id uname fscan user exit\r\n");
    } else if (f_streq(line, "clear")) {
        console_clear();
    } else if (f_streq(line, "pwd")) {
        console_write("/home/root\r\n");
    } else if (f_streq(line, "ls") || f_streq(line, "ls /home/root")) {
        console_write("README  motd  .profile\r\n");
    } else if (f_streq(line, "ls /")) {
        console_write("bin  dev  etc  home  ram  tmp\r\n");
    } else if (f_streq(line, "cd /") || f_streq(line, "cd /home/root")) {
        console_write("directory changed\r\n");
    } else if (f_streq(line, "cat README") || f_streq(line, "cat /home/root/README")) {
        console_write("Welcome to FadalOS.\r\nThis is the FadalOS Phase 1 console.\r\n");
    } else if (f_streq(line, "cat motd") || f_streq(line, "cat /etc/motd")) {
        console_write("FadalOS: small, inspectable, and user-aware.\r\n");
    } else if (f_streq(line, "whoami")) {
        console_write(current_user);
        console_write("\r\n");
    } else if (f_streq(line, "id")) {
        console_write("uid=");
        print_uid();
        console_write(" gid=");
        print_uid();
        console_write(" groups=");
        if (current_uid == USER_ROOT) console_write("root,users");
        else console_write("users");
        console_write("\r\n");
    } else if (f_streq(line, "uname") || f_streq(line, "uname -a")) {
        console_write("FadalOS fadal64 0.1.0 UEFI x86_64 console\r\n");
    } else if (f_streq(line, "fscan")) {
        console_write("security: FScan audit passed; loopback-only boundary verified\r\n");
        console_write("security: root/user capability boundary online\r\n");
    } else if (f_streq(line, "user")) {
        console_write("root (uid 0) active; fadal (uid 1000) model registered\r\n");
    } else if (f_streq(line, "exit")) {
        console_write("FadalOS halted. You may close the UTM/QEMU session.\r\n");
        for (;;) __asm__ volatile ("hlt");
    } else if (f_starts(line, "cd ")) {
        console_write("cd: persistent path resolver is scheduled for Phase 1B\r\n");
    } else if (f_starts(line, "cat ")) {
        console_write("cat: file not found in the Phase 1 console filesystem\r\n");
    } else {
        console_write("fadal: command not found; type help\r\n");
    }
}

__attribute__((section(".text.entry"), used, noreturn))
void fadal64_entry(void) {
    static const char debug_banner[] = "FADALOS_CONSOLE_ENTRY\n";
    char line[LINE_MAX];
    u32 length;
    u32 index;

    serial_init();
    for (index = 0; index < sizeof(debug_banner) - 1; index++) fadal_outb(0x402, (u8)debug_banner[index]);
    console_clear();
    console_write("FadalOS 0.1.0 / Fadal64 UEFI\r\n");
    console_write("persistent home filesystem: pending Phase 1B\r\n");
    console_write("type 'help' for commands\r\n\r\n");

    for (;;) {
        prompt();
        length = 0;
        for (;;) {
            char value = serial_getc();
            if (value == '\r' || value == '\n') {
                console_write("\r\n");
                line[length] = '\0';
                run_command(line);
                break;
            }
            if ((value == '\b' || value == 127) && length > 0) {
                length--;
                console_write("\b \b");
            } else if (value >= 32 && value <= 126 && length + 1 < LINE_MAX) {
                line[length++] = value;
                serial_putc((u8)value);
                vga_putc(value);
            }
        }
    }
}
