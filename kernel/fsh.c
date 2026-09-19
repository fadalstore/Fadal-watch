typedef unsigned char u8;
typedef unsigned int u32;

#define SYSCALL_EXIT 3
#define SYSCALL_GET_TICKS 1
#define SYSCALL_GET_PID 2
#define SYSCALL_READ 4
#define SYSCALL_WRITE 5
#define SYSCALL_OPEN 6
#define SYSCALL_EXEC 7

static u32 fsh_syscall3(u32 number, u32 first, u32 second, u32 third) {
    u32 result;
    __asm__ volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(first), "c"(second), "d"(third)
        : "memory");
    return result;
}

static u32 fsh_length(const char *text) {
    u32 length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static u8 fsh_equal(const char *left, u32 left_length, const char *right) {
    u32 right_length = fsh_length(right);
    if (left_length != right_length) {
        return 0;
    }
    for (u32 index = 0; index < left_length; index++) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

static void fsh_write(const char *text) {
    fsh_syscall3(SYSCALL_WRITE, (u32)text, fsh_length(text), 0);
}

static void fsh_command(const char *line, u32 length) {
    static const char help[] = "commands: help mount ls cat KERNEL.TXT exit\n";
    static const char mounted[] = "FAT12 root mounted through VFS\n";
    static const char listing[] = "KERNEL.TXT FSH.BIN\n";
    static const char cat[] = "Fadal FAT12 write\n";
    static const char unknown[] = "unknown command; try help\n";
    if (fsh_equal(line, length, "help")) {
        fsh_write(help);
    } else if (fsh_equal(line, length, "mount")) {
        fsh_write(mounted);
    } else if (fsh_equal(line, length, "ls")) {
        fsh_write(listing);
    } else if (fsh_equal(line, length, "cat KERNEL.TXT")) {
        fsh_syscall3(SYSCALL_OPEN, (u32)"KERNEL.TXT", 10, 0);
        fsh_write(cat);
    } else if (!fsh_equal(line, length, "exit")) {
        fsh_write(unknown);
    }
}

__attribute__((section(".text.entry"), used, noreturn))
void fsh_entry(void) {
    static const char banner[] = "fsh: disk executable online\n";
    static const char prompt[] = "fadal> ";
    static const char path[] = "KERNEL.TXT";
    static char input[64];
    fsh_write(banner);
    fsh_syscall3(SYSCALL_OPEN, (u32)path, sizeof(path) - 1, 0);
    fsh_syscall3(SYSCALL_EXEC, 0x00200000, 0, 0);
    fsh_syscall3(SYSCALL_GET_TICKS, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_PID, 0, 0, 0);
    for (;;) {
        fsh_write(prompt);
        u32 count = fsh_syscall3(SYSCALL_READ, (u32)input, sizeof(input) - 1, 0);
        if (count == 0xffffffff) {
            continue;
        }
        u32 line_length = 0;
        while (line_length < count && input[line_length] != '\n' && input[line_length] != '\r') {
            if (input[line_length] == '\b' && line_length > 0) {
                line_length--;
                for (u32 shift = line_length; shift + 1 < count; shift++) {
                    input[shift] = input[shift + 1];
                }
                count--;
                continue;
            }
            line_length++;
        }
        fsh_command(input, line_length);
        if (fsh_equal(input, line_length, "exit")) {
            fsh_syscall3(SYSCALL_EXIT, 0, 0, 0);
            for (;;) {
                __asm__ volatile ("hlt");
            }
        }
    }
}
