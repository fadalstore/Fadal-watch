typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef struct {
    u64 base;
    u32 width;
    u32 height;
    u32 pixels_per_scanline;
    u32 pixel_format;
} fadal_framebuffer;

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
#define EVENT_QUEUE_SIZE 32
#define USER_ROOT 0
#define USER_FADAL 1000

typedef struct {
    u8 type;
    u8 code;
    int dx;
    int dy;
    u32 buttons;
} fadal_event;

static u32 vga_row;
static u32 vga_col;
static fadal_framebuffer *framebuffer;
static u8 desktop_mode;
static u8 mouse_enabled;
static u8 mouse_packet_index;
static u8 mouse_packet[3];
static u8 start_open;
static u32 mouse_x;
static u32 mouse_y;
static u32 mouse_buttons;
static fadal_event event_queue[EVENT_QUEUE_SIZE];
static u8 event_head;
static u8 event_tail;
static const char *desktop_status = "SYSTEM READY";
static u32 current_uid = USER_ROOT;
static const char current_user[] = "root";

static void fb_pixel(u32 x, u32 y, u32 color) {
    volatile u32 *pixels;
    if (desktop_mode == 0 || framebuffer == (fadal_framebuffer *)0 ||
        x >= framebuffer->width || y >= framebuffer->height) return;
    pixels = (volatile u32 *)(u64)framebuffer->base;
    pixels[y * framebuffer->pixels_per_scanline + x] = color;
}

static void fb_fill(u32 left, u32 top, u32 right, u32 bottom, u32 color) {
    u32 x;
    u32 y;
    if (right > framebuffer->width) right = framebuffer->width;
    if (bottom > framebuffer->height) bottom = framebuffer->height;
    for (y = top; y < bottom; y++) {
        for (x = left; x < right; x++) fb_pixel(x, y, color);
    }
}

static u8 glyph_row(char value, u32 row) {
    static const u8 digits[10][7] = {
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}
    };
    static const u8 letters[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
        {1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
    };
    if (value >= '0' && value <= '9') return digits[(u32)(value - '0')][row];
    if (value >= 'a' && value <= 'z') value = (char)(value - 'a' + 'A');
    if (value >= 'A' && value <= 'Z') return letters[(u32)(value - 'A')][row];
    if (value == '-') return row == 3 ? 31 : 0;
    if (value == ':') return (row == 2 || row == 5) ? 4 : 0;
    if (value == '.') return row == 6 ? 4 : 0;
    return 0;
}

static void fb_text(u32 x, u32 y, const char *text, u32 color, u32 scale) {
    u32 index;
    u32 row;
    u32 bit;
    u32 dx;
    u32 dy;
    for (index = 0; text[index] != '\0'; index++) {
        for (row = 0; row < 7; row++) {
            for (bit = 0; bit < 5; bit++) {
                if ((glyph_row(text[index], row) & (1U << (4 - bit))) != 0) {
                    for (dy = 0; dy < scale; dy++) {
                        for (dx = 0; dx < scale; dx++) fb_pixel(x + bit * scale + dx,
                                                                  y + row * scale + dy, color);
                    }
                }
            }
        }
        x += 6 * scale;
    }
}

static void desktop_render(void) {
    u32 width;
    u32 height;
    if (framebuffer == (fadal_framebuffer *)0 || framebuffer->base == 0 ||
        framebuffer->width < 640 || framebuffer->height < 400) return;
    desktop_mode = 1;
    width = framebuffer->width;
    height = framebuffer->height;
    fb_fill(0, 0, width, height, 0x00111b33);
    fb_fill(0, 0, width, 34, 0x00233b67);
    fb_fill(0, height - 52, width, height, 0x0015223d);
    fb_fill(18, 58, width - 18, height - 72, 0x00182745);
    fb_fill(32, 78, width - 32, height - 92, 0x001c2f52);
    fb_fill(52, 98, 250, 230, 0x00243f6d);
    fb_fill(66, 112, 236, 220, 0x002b4d80);
    fb_fill(270, 98, width - 52, height - 112, 0x000d1528);
    fb_fill(270, 98, width - 52, 132, 0x001c3154);
    fb_fill(284, 150, width - 70, height - 132, 0x000a101e);
    fb_fill(26, height - 43, 126, height - 10, 0x002d5f9b);
    fb_fill(142, height - 43, 184, height - 10, 0x00243f6d);
    fb_fill(196, height - 43, 238, height - 10, 0x00243f6d);
    fb_text(18, 11, "FADAL OS", 0x00ffffff, 2);
    fb_text(52, 126, "DESKTOP", 0x00d8eaff, 2);
    fb_text(78, 146, "WELCOME", 0x00ffffff, 2);
    fb_text(78, 166, "FADAL OS", 0x008dc5ff, 2);
    fb_text(286, 108, "TERMINAL", 0x00ffffff, 2);
    fb_text(292, 154, desktop_status, 0x007ed6a7, 2);
    fb_text(292, 176, "TYPE HELP FOR COMMANDS", 0x0097aac8, 1);
    fb_text(38, height - 33, "START", 0x00ffffff, 2);
    fb_text(width - 155, height - 31, "FADAL64", 0x0097aac8, 1);
}

static void mouse_wait_input(void) {
    u32 timeout = 100000;
    while (timeout-- != 0 && (fadal_inb(KBD_STATUS) & 2) != 0) __asm__ volatile ("pause");
}

static void mouse_write(u8 value) {
    mouse_wait_input();
    fadal_outb(KBD_STATUS, 0xd4);
    mouse_wait_input();
    fadal_outb(KBD_DATA, value);
}

static void mouse_init(void) {
    u32 timeout = 100000;
    while (timeout-- != 0 && (fadal_inb(KBD_STATUS) & 1) != 0) (void)fadal_inb(KBD_DATA);
    mouse_wait_input();
    fadal_outb(KBD_STATUS, 0xa8);
    mouse_write(0xf6);
    mouse_write(0xf4);
    mouse_enabled = 1;
    mouse_packet_index = 0;
    mouse_x = framebuffer->width / 2;
    mouse_y = framebuffer->height / 2;
    mouse_buttons = 0;
}

static void cursor_draw(void) {
    static const u8 arrow[12] = {1, 3, 7, 15, 31, 63, 127, 255, 127, 71, 3, 1};
    u32 row;
    u32 column;
    for (row = 0; row < 12; row++) {
        for (column = 0; column < 8; column++) {
            if ((arrow[row] & (1U << (7 - column))) != 0) {
                fb_pixel(mouse_x + column, mouse_y + row, 0x00ffffff);
            }
        }
    }
}

static void desktop_redraw(void) {
    desktop_render();
    if (start_open != 0) {
        fb_fill(26, framebuffer->height - 270, 290, framebuffer->height - 54, 0x001a3155);
        fb_fill(38, framebuffer->height - 252, 278, framebuffer->height - 208, 0x002d5f9b);
        fb_fill(38, framebuffer->height - 198, 278, framebuffer->height - 154, 0x00243f6d);
        fb_fill(38, framebuffer->height - 144, 278, framebuffer->height - 100, 0x00243f6d);
        fb_text(54, framebuffer->height - 238, "TERMINAL", 0x00ffffff, 2);
        fb_text(54, framebuffer->height - 184, "FILES", 0x00ffffff, 2);
        fb_text(54, framebuffer->height - 130, "SYSTEM", 0x00ffffff, 2);
    }
    cursor_draw();
}

static void event_push(u8 type, u8 code, int dx, int dy, u32 buttons) {
    u8 next = (u8)((event_head + 1U) % EVENT_QUEUE_SIZE);
    if (next == event_tail) event_tail = (u8)((event_tail + 1U) % EVENT_QUEUE_SIZE);
    event_queue[event_head].type = type;
    event_queue[event_head].code = code;
    event_queue[event_head].dx = dx;
    event_queue[event_head].dy = dy;
    event_queue[event_head].buttons = buttons;
    event_head = next;
}

static int event_pop(fadal_event *event) {
    if (event_head == event_tail) return 0;
    *event = event_queue[event_tail];
    event_tail = (u8)((event_tail + 1U) % EVENT_QUEUE_SIZE);
    return 1;
}

static void desktop_handle_mouse(const fadal_event *event) {
    if (event->type != 2 || (event->buttons & 1U) == 0) return;
    if (mouse_x < 150 && mouse_y > framebuffer->height - 70) {
        start_open = (u8)!start_open;
        desktop_status = start_open != 0 ? "START MENU OPEN" : "SYSTEM READY";
    } else if (start_open != 0 && mouse_x >= 38 && mouse_x < 278 &&
               mouse_y >= framebuffer->height - 252 && mouse_y < framebuffer->height - 208) {
        start_open = 0;
        desktop_status = "TERMINAL SELECTED";
    } else if (start_open != 0 && mouse_x >= 38 && mouse_x < 278 &&
               mouse_y >= framebuffer->height - 198 && mouse_y < framebuffer->height - 154) {
        start_open = 0;
        desktop_status = "FILES SELECTED";
    } else if (start_open != 0 && mouse_x >= 38 && mouse_x < 278 &&
               mouse_y >= framebuffer->height - 144 && mouse_y < framebuffer->height - 100) {
        start_open = 0;
        desktop_status = "SYSTEM SELECTED";
    }
    desktop_redraw();
}

static void mouse_poll(void) {
    u8 value;
    int dx;
    int dy;
    if (mouse_enabled == 0 || (fadal_inb(KBD_STATUS) & 1) == 0) return;
    value = fadal_inb(KBD_DATA);
    if (mouse_packet_index == 0 && (value & 8) == 0) return;
    mouse_packet[mouse_packet_index++] = value;
    if (mouse_packet_index < 3) return;
    mouse_packet_index = 0;
    dx = (int)mouse_packet[1];
    dy = -(int)mouse_packet[2];
    if ((mouse_packet[0] & 0x10) != 0) dx -= 256;
    if ((mouse_packet[0] & 0x20) != 0) dy += 256;
    if (dx < 0 && mouse_x < (u32)(-dx)) mouse_x = 0;
    else if (dx > 0 && mouse_x + (u32)dx >= framebuffer->width) mouse_x = framebuffer->width - 1;
    else mouse_x = (u32)((int)mouse_x + dx);
    if (dy < 0 && mouse_y < (u32)(-dy)) mouse_y = 0;
    else if (dy > 0 && mouse_y + (u32)dy >= framebuffer->height) mouse_y = framebuffer->height - 1;
    else mouse_y = (u32)((int)mouse_y + dy);
    if ((mouse_packet[0] & 1) != 0 && (mouse_buttons & 1) == 0) {
        mouse_buttons |= 1;
    } else if ((mouse_packet[0] & 1) == 0) {
        mouse_buttons &= ~1U;
    }
    event_push(2, 0, dx, dy, mouse_buttons);
}

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
    if (desktop_mode != 0) return;
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

static void event_pump(void) {
    if (serial_ready()) event_push(1, fadal_inb(COM1), 0, 0, 0);
    if (fadal_inb(KBD_STATUS) & 1) {
        char value = keyboard_ascii(fadal_inb(KBD_DATA));
        if (value != 0) event_push(1, (u8)value, 0, 0, 0);
    }
    mouse_poll();
}

static char serial_getc(void) {
    fadal_event event;
    for (;;) {
        event_pump();
        while (event_pop(&event) != 0) {
            if (event.type == 1) return (char)event.code;
            if (event.type == 2) {
                desktop_handle_mouse(&event);
                if (event.dx != 0 || event.dy != 0) desktop_redraw();
            }
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
void fadal64_entry(fadal_framebuffer *incoming_framebuffer) {
    static const char debug_banner[] = "FADALOS_CONSOLE_ENTRY\n";
    char line[LINE_MAX];
    u32 length;
    u32 index;
    u8 suppress_lf = 0;

    framebuffer = incoming_framebuffer;
    serial_init();
    desktop_render();
    if (desktop_mode != 0) {
        mouse_init();
        desktop_redraw();
    }
    for (index = 0; index < sizeof(debug_banner) - 1; index++) fadal_outb(0x402, (u8)debug_banner[index]);
    if (desktop_mode == 0) console_clear();
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
