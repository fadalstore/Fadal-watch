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

__attribute__((section(".text.entry"), used, noreturn))
void fsh_entry(void) {
    static const char banner[] = "fsh: disk executable online\n";
    static const char path[] = "KERNEL.TXT";
    char input[32];
    fsh_syscall3(SYSCALL_READ, (u32)input, sizeof(input), 0);
    fsh_syscall3(SYSCALL_WRITE, (u32)banner, sizeof(banner) - 1, 0);
    fsh_syscall3(SYSCALL_OPEN, (u32)path, sizeof(path) - 1, 0);
    fsh_syscall3(SYSCALL_EXEC, 0x00200000, 0, 0);
    fsh_syscall3(SYSCALL_GET_TICKS, 0, 0, 0);
    fsh_syscall3(SYSCALL_GET_PID, 0, 0, 0);
    fsh_syscall3(SYSCALL_EXIT, 0, 0, 0);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
