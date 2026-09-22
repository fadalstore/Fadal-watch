typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

static inline void fadal_outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* The image header is patched with the final binary size by the build tool. */
__attribute__((section(".fadal_header"), used))
const u8 fadal64_header[32] = {
    0x34, 0x36, 0x4c, 0x44, 0x46, 0x00, 0x00, 0x00, /* FDL64 */
    0x01, 0x00, 0x00, 0x00,                         /* version */
    0x20, 0x00, 0x00, 0x00,                         /* header_size */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* image_size */
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* entry_offset */
};

__attribute__((section(".text.entry"), used, noreturn))
void fadal64_entry(void) {
    static const char serial_banner[] = "FADAL64 UEFI PAYLOAD ONLINE\r\n";
    static const char debug_banner[] = "FADAL64_ENTRY\n";
    volatile u16 *vga = (volatile u16 *)0xb8000;
    static const char banner[] = "FADAL64 UEFI PAYLOAD ONLINE";
    for (u32 index = 0; index < sizeof(debug_banner) - 1; index++) {
        fadal_outb(0x402, (u8)debug_banner[index]);
    }
    for (u32 index = 0; index < sizeof(serial_banner) - 1; index++) {
        fadal_outb(0x3f8, (u8)serial_banner[index]);
    }
    for (u32 index = 0; index < sizeof(banner) - 1; index++) {
        vga[index] = (u16)((u8)banner[index] | (0x1f << 8));
    }
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
