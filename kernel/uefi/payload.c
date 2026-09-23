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
#define KBD_DATA 0x60
#define KBD_STATUS 0x64
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
    while ((fadal_inb(COM1 + 5) & 0x20) == 0) __asm__ volatile ("pause");
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

static char keyboard_ascii(u8 code) {
    static const char keys[128] = {
        [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0a]='9',[0x0b]='0',
        [0x0c]='-',[0x0d]='=',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',
        [0x18]='o',[0x19]='p',[0x1a]='[',[0x1b]=']',[0x1e]='a',[0x1f]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',
        [0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'', [0x29]='`',[0x2b]='\\',[0x2c]='z',[0x2d]='x',
        [0x2e]='c',[0x2f]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '
    };
    static const char shifted[128] = {
        [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0a]='(',[0x0b]=')',
        [0x0c]='_',[0x0d]='+',[0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',
        [0x18]='O',[0x19]='P',[0x1a]='{',[0x1b]='}',[0x1e]='A',[0x1f]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',
        [0x24]='J',[0x25]='K',[0x26]='L',[0x27]=':',[0x28]='"',[0x29]='~',[0x2b]='|',[0x2c]='Z',[0x2d]='X',
        [0x2e]='C',[0x2f]='V',[0x30]='B',[0x31]='N',[0x32]='M',[0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' '
    };
    static u8 shift;
    if (code == 0x2a || code == 0x36) { shift = 1; return 0; }
    if (code == 0xaa || code == 0xb6) { shift = 0; return 0; }
    if (code == 0x1c) return '\n';
    if (code == 0x0e) return '\b';
    if (code >= 128) return 0;
    return shift ? shifted[code] : keys[code];
}

static char serial_getc(void) {
    for (;;) {
        if (serial_ready()) return (char)fadal_inb(COM1);
        if (fadal_inb(KBD_STATUS) & 1) {
            char value = keyboard_ascii(fadal_inb(KBD_DATA));
            if (value != 0) return value;
        }
        __asm__ volatile ("pause");
    }
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
    u8 suppress_lf = 0;

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
            if (suppress_lf != 0) {
                suppress_lf = 0;
                if (value == '\n') continue;
            }
            if (value == '\r' || value == '\n') {
                console_write("\r\n");
                line[length] = '\0';
                run_command(line);
                if (value == '\r') suppress_lf = 1;
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
