typedef unsigned char u8;
typedef unsigned int u32;

#define SYSCALL_EXIT 3
#define SYSCALL_GET_TICKS 1
#define SYSCALL_GET_PID 2
#define SYSCALL_GET_PPID 13
#define SYSCALL_SLEEP 14
#define SYSCALL_MMAP 15
#define SYSCALL_GET_RING 16
#define SYSCALL_GET_DEVICE 17
#define SYSCALL_SET_PRIORITY 18
#define SYSCALL_NET_LOOPBACK 19
#define SYSCALL_GET_CPUS 20
#define SYSCALL_GET_IPI 21
#define SYSCALL_GET_SYNC 22
#define SYSCALL_GET_FS 23
#define SYSCALL_GET_LOG 24
#define SYSCALL_GET_RAMDISK 25
#define SYSCALL_GET_SECURITY 26
#define SYSCALL_GETUID 28
#define SYSCALL_GETGID 29
#define SYSCALL_GETCAPS 30
#define SYSCALL_READ 4
#define SYSCALL_WRITE 5
#define SYSCALL_OPEN 6
#define SYSCALL_EXEC 7
#define SYSCALL_YIELD 8
#define SYSCALL_CLOSE 9
#define SYSCALL_STAT 10
#define SYSCALL_SEEK 11
#define SYSCALL_WAIT 12
#define SYSCALL_LISTDIR 27
#define SYSCALL_LOGIN 31
#define SYSCALL_CHANGE_PASSWORD 32
#define SYSCALL_GET_ROLE 33
#define SYSCALL_SET_TTY_ECHO 34

static u32 fsh_syscall3(u32 number, u32 first, u32 second, u32 third) {
    u32 result;
    __asm__ volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(first), "c"(second), "d"(third)
        : "memory");
    return result;
}
static u32 fsh_syscall4(u32 number, u32 first, u32 second, u32 third, u32 fourth) {
    u32 result;
    __asm__ volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(first), "c"(second), "d"(third), "S"(fourth)
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
    fsh_syscall3(SYSCALL_WRITE, (u32)text, fsh_length(text), 1);
}

static void fsh_write_bytes(const char *text, u32 length) {
    fsh_syscall3(SYSCALL_WRITE, (u32)text, length, 1);
}
static u32 fsh_copy_line(char *destination, u32 capacity, const char *source, u32 length) {
    if (length >= capacity) return 0;
    for (u32 index = 0; index < length; index++) destination[index] = source[index];
    destination[length] = '\0';
    return length;
}

static void fsh_stat_kernel(void) {
    static const char path[] = "KERNEL.TXT";
    static const char result[] = "KERNEL.TXT: regular file\n";
    static u32 metadata[2];
    if (fsh_syscall3(SYSCALL_STAT, (u32)path, sizeof(path) - 1,
                     (u32)metadata) == 0) {
        fsh_write(result);
    }
}

static void fsh_seek_probe(void) {
    static const char path[] = "KERNEL.TXT";
    static char first_byte[1];
    u32 fd = fsh_syscall3(SYSCALL_OPEN, (u32)path, sizeof(path) - 1, 0);
    if (fd != 0xffffffff &&
        fsh_syscall3(SYSCALL_SEEK, fd, 0, 0) == 0) {
        fsh_syscall3(SYSCALL_READ, (u32)first_byte, sizeof(first_byte), fd);
        fsh_syscall3(SYSCALL_CLOSE, fd, 0, 0);
    }
}

static u32 fsh_list_root(char *output, u32 capacity) {
    return fsh_syscall3(SYSCALL_LISTDIR, (u32)output, capacity, 0);
}

static void fsh_command(const char *line, u32 length) {
    static const char help[] = "commands: help login passwd whoami mount ls cat stat fscan github KERNEL.TXT exit\n";
    static const char mounted[] = "FAT12 root mounted through VFS\n";
    static const char unknown[] = "unknown command; try help\n";
    static const char github[] = "GitHub: https://github.com/fadalstore/Fadal-watch\nHost terminal: gh repo clone fadalstore/Fadal-watch\n";
    static const char fscan_passed[] = "FScan: VFS, FAT12, RAMFS, and loopback boundary passed\n";
    static const char fscan_failed[] = "FScan: security audit reported an integrity issue\n";
    static u32 auth_stage;
    static char login_user[8];
    static char old_password[32];
    if (auth_stage == 1) {
        if (fsh_equal(line, length, "root") || fsh_equal(line, length, "user")) {
            fsh_copy_line(login_user, sizeof(login_user), line, length);
            auth_stage = 2;
            fsh_syscall3(SYSCALL_SET_TTY_ECHO, 0, 0, 0);
            fsh_write("password: ");
        } else {
            auth_stage = 0;
            fsh_write("login: unknown user\n");
        }
        return;
    }
    if (auth_stage == 2) {
        fsh_syscall3(SYSCALL_SET_TTY_ECHO, 1, 0, 0);
        fsh_write(fsh_syscall4(SYSCALL_LOGIN, (u32)login_user, fsh_length(login_user),
                               (u32)line, length) ? "login: authenticated\n" : "login: failed\n");
        auth_stage = 0;
        return;
    }
    if (auth_stage == 3) {
        if (!fsh_copy_line(old_password, sizeof(old_password), line, length)) {
            fsh_write("passwd: password too long\n"); auth_stage = 0; return;
        }
        auth_stage = 4;
        fsh_write("new password: ");
        return;
    }
    if (auth_stage == 4) {
        fsh_syscall3(SYSCALL_SET_TTY_ECHO, 1, 0, 0);
        fsh_write(fsh_syscall4(SYSCALL_CHANGE_PASSWORD, (u32)old_password, fsh_length(old_password),
                               (u32)line, length) ? "password: changed\n" : "password: change failed\n");
        auth_stage = 0;
        return;
    }
    if (fsh_equal(line, length, "help")) {
        fsh_write(help);
    } else if (fsh_equal(line, length, "mount")) {
        fsh_write(mounted);
    } else if (fsh_equal(line, length, "ls")) {
        static char listing[128];
        u32 count = fsh_list_root(listing, sizeof(listing));
        if (count != 0xffffffff) {
            fsh_write_bytes(listing, count);
        }
    } else if (fsh_equal(line, length, "cat KERNEL.TXT")) {
        u32 fd = fsh_syscall3(SYSCALL_OPEN, (u32)"KERNEL.TXT", 10, 0);
        if (fd != 0xffffffff) {
            static char contents[64];
            u32 count = fsh_syscall3(SYSCALL_READ, (u32)contents, sizeof(contents), fd);
            if (count != 0xffffffff) {
                fsh_write_bytes(contents, count);
            }
            fsh_syscall3(SYSCALL_CLOSE, fd, 0, 0);
        }
    } else if (fsh_equal(line, length, "stat KERNEL.TXT")) {
        fsh_stat_kernel();
    } else if (fsh_equal(line, length, "fscan")) {
        fsh_write(fsh_syscall3(SYSCALL_GET_SECURITY, 0, 0, 0) == 0x0f ?
            fscan_passed : fscan_failed);
    } else if (fsh_equal(line, length, "github")) {
        fsh_write(github);
    } else if (fsh_equal(line, length, "login")) {
        auth_stage = 1;
        fsh_write("login: username: ");
    } else if (fsh_equal(line, length, "whoami")) {
        fsh_write(fsh_syscall3(SYSCALL_GET_ROLE, 0, 0, 0) == 2 ? "root\n" : "user\n");
    } else if (fsh_equal(line, length, "passwd")) {
        if (fsh_syscall3(SYSCALL_GET_ROLE, 0, 0, 0) == 0) {
            fsh_write("passwd: login required\n");
        } else {
            auth_stage = 3;
            fsh_syscall3(SYSCALL_SET_TTY_ECHO, 0, 0, 0);
            fsh_write("old password: ");
        }
    } else if (!fsh_equal(line, length, "exit")) {
        fsh_write(unknown);
    }
}

__attribute__((section(".text.entry"), used, noreturn))
void fsh_entry(void) {
    static const char banner[] = "fsh: disk executable online\n";
    static const char user_prompt[] = "fadal$ ";
    static const char root_prompt[] = "fadal# ";
    static const char path[] = "KERNEL.TXT";
    static char input[64];
    u8 prompt_pending = 1;
    fsh_write(banner);
    u32 boot_fd = fsh_syscall3(SYSCALL_OPEN, (u32)path, sizeof(path) - 1, 0);
    if (boot_fd != 0xffffffff) {
        fsh_syscall3(SYSCALL_CLOSE, boot_fd, 0, 0);
    }
    fsh_stat_kernel();
    fsh_seek_probe();
    static char root_probe[128];
    fsh_list_root(root_probe, sizeof(root_probe));
    u32 child_pid = fsh_syscall3(SYSCALL_EXEC, 0x00200000, 0, 0);
    static u32 child_status;
    if (child_pid != 0xffffffff) {
        fsh_syscall3(SYSCALL_WAIT, child_pid, (u32)&child_status, 0);
    }
    fsh_syscall3(SYSCALL_GET_TICKS, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_PID, 0, 0, 0);
    fsh_syscall3(SYSCALL_GETUID, 0, 0, 0);
    fsh_syscall3(SYSCALL_GETGID, 0, 0, 0);
    fsh_syscall3(SYSCALL_GETCAPS, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_PPID, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_RING, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_DEVICE, 0, 0, 0);
    fsh_syscall3(SYSCALL_SET_PRIORITY, 2, 0, 0);
    fsh_syscall3(SYSCALL_NET_LOOPBACK, (u32)user_prompt, 7, 0);
    fsh_syscall3(SYSCALL_NET_LOOPBACK, (u32)input, sizeof(input), 1);
    fsh_syscall3(SYSCALL_GET_CPUS, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_IPI, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_SYNC, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_FS, 2, 0, 0);
    fsh_syscall3(SYSCALL_GET_LOG, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_RAMDISK, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_SECURITY, 0, 0, 0);
    fsh_syscall3(SYSCALL_SLEEP, 1, 0, 0);
    fsh_syscall3(SYSCALL_MMAP, 32, 0, 0);
    for (;;) {
        if (prompt_pending) {
            fsh_write(fsh_syscall3(SYSCALL_GET_ROLE, 0, 0, 0) == 2 ? root_prompt : user_prompt);
            prompt_pending = 0;
        }
        u32 count = fsh_syscall3(SYSCALL_READ, (u32)input, sizeof(input) - 1, 0);
        if (count == 0xffffffff) {
            fsh_syscall3(SYSCALL_YIELD, 0, 0, 0);
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
        prompt_pending = 1;
        if (fsh_equal(input, line_length, "exit")) {
            fsh_syscall3(SYSCALL_EXIT, 0, 0, 0);
            for (;;) {
                __asm__ volatile ("hlt");
            }
        }
    }
}
