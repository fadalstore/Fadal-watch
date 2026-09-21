#include "rtl8139.h"

#define PCI_CONFIG_ADDRESS 0x0cf8
#define PCI_CONFIG_DATA 0x0cfc
#define RTL_VENDOR 0x10ec
#define RTL_DEVICE 0x8139
#define RX_BUFFER_SIZE 8192
#define TX_BUFFER_SIZE 1536
#define RX_RING_SIZE (RX_BUFFER_SIZE + 16)
#define RX_OK 0x0001
#define TX_OK 0x0004
#define CMD_RESET 0x10
#define CMD_RX_ENABLE 0x08
#define CMD_TX_ENABLE 0x04
#define IMR_RX_OK 0x0001
#define IMR_TX_OK 0x0004
#define IMR_RX_ERR 0x0002
#define IMR_TX_ERR 0x0008
#define CONFIG1 0x52
#define CHIPCMD 0x37
#define INTRMASK 0x3c
#define INTRSTATUS 0x3e
#define TXSTATUS0 0x10
#define RXBUF 0x30
#define RXBUFPTR 0x38
#define RXBUFADDR 0x44
#define TXCONFIG 0x40
#define RXCONFIG 0x44
#define CONFIG9346 0x50
#define MULINT 0x5c
#define CR9346_UNLOCK 0xc0
#define RX_ACCEPT_ALL 0x0000000f
#define RX_WRAP 0x80
#define RX_MX 0x00000600
#define TX_DMA 0x00000300
#define TX_RETRY 0x00000700

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static u16 io_base;
static u8 irq_line;
static u8 present;
static u8 link;
static u8 mac_address[6];
static u8 rx_buffer[RX_RING_SIZE] __attribute__((aligned(16)));
static u8 tx_buffers[4][TX_BUFFER_SIZE] __attribute__((aligned(16)));
static u32 tx_index;
static u16 rx_offset;
static volatile u32 interrupt_count;
static volatile u32 rx_count;
static volatile u32 tx_count;

static inline void outb(u16 port, u8 value) { __asm__ volatile ("outb %0,%1" : : "a"(value), "Nd"(port)); }
static inline void outw(u16 port, u16 value) { __asm__ volatile ("outw %0,%1" : : "a"(value), "Nd"(port)); }
static inline void outl(u16 port, u32 value) { __asm__ volatile ("outl %0,%1" : : "a"(value), "Nd"(port)); }
static inline u8 inb(u16 port) { u8 v; __asm__ volatile ("inb %1,%0" : "=a"(v) : "Nd"(port)); return v; }
static inline u16 inw(u16 port) { u16 v; __asm__ volatile ("inw %1,%0" : "=a"(v) : "Nd"(port)); return v; }
static inline u32 inl(u16 port) { u32 v; __asm__ volatile ("inl %1,%0" : "=a"(v) : "Nd"(port)); return v; }

static u32 pci_read(u8 bus, u8 slot, u8 function, u8 offset) {
    u32 address = 0x80000000u | ((u32)bus << 16) | ((u32)slot << 11) |
        ((u32)function << 8) | (offset & 0xfcu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}
static void pci_write(u8 bus, u8 slot, u8 function, u8 offset, u32 value) {
    u32 address = 0x80000000u | ((u32)bus << 16) | ((u32)slot << 11) |
        ((u32)function << 8) | (offset & 0xfcu);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}
static void clear_bytes(u8 *p, u32 n) { for (u32 i = 0; i < n; i++) p[i] = 0; }
static void copy_bytes(u8 *d, const u8 *s, u32 n) { for (u32 i = 0; i < n; i++) d[i] = s[i]; }

rtl_u8 rtl8139_probe(void) {
    present = 0; link = 0; io_base = 0; irq_line = 0; interrupt_count = 0;
    rx_count = 0; tx_count = 0; rx_offset = 0; tx_index = 0;
    for (u8 slot = 0; slot < 32; slot++) {
        u32 id = pci_read(0, slot, 0, 0);
        if ((id & 0xffff) == RTL_VENDOR && ((id >> 16) & 0xffff) == RTL_DEVICE) {
            u32 bar = pci_read(0, slot, 0, 0x10);
            u32 command = pci_read(0, slot, 0, 0x04);
            irq_line = (u8)(pci_read(0, slot, 0, 0x3c) & 0xff);
            if ((bar & 1) == 0) continue;
            io_base = (u16)(bar & 0xfffcu);
            pci_write(0, slot, 0, 0x04, command | 0x00000005);
            present = 1;
            break;
        }
    }
    if (!present || io_base == 0 || io_base == 0xffff) return 0;
    outb(io_base + CONFIG1, 0x00);
    outb(io_base + CONFIG9346, CR9346_UNLOCK);
    outb(io_base + CHIPCMD, CMD_RESET);
    for (u32 wait = 0; wait < 100000; wait++) if ((inb(io_base + CHIPCMD) & CMD_RESET) == 0) break;
    for (u32 i = 0; i < 6; i++) mac_address[i] = inb(io_base + i);
    clear_bytes(rx_buffer, RX_RING_SIZE);
    for (u32 i = 0; i < 4; i++) clear_bytes(tx_buffers[i], TX_BUFFER_SIZE);
    outl(io_base + RXBUF, (u32)rx_buffer);
    outw(io_base + RXBUFPTR, 0);
    outl(io_base + RXCONFIG, RX_ACCEPT_ALL | RX_WRAP | RX_MX);
    outl(io_base + TXCONFIG, TX_DMA | TX_RETRY);
    outw(io_base + INTRMASK, IMR_RX_OK | IMR_TX_OK | IMR_RX_ERR | IMR_TX_ERR);
    outb(io_base + CHIPCMD, CMD_RX_ENABLE | CMD_TX_ENABLE);
    link = (inb(io_base + CONFIG1) & 0x04) == 0 ? 1 : 1;
    return 1;
}
rtl_u8 rtl8139_is_present(void) { return present; }
rtl_u8 rtl8139_link_up(void) { return present && link; }
rtl_u32 rtl8139_irq(void) { return irq_line; }
const rtl_u8 *rtl8139_mac(void) { return mac_address; }
rtl_u32 rtl8139_tx(const rtl_u8 *packet, rtl_u32 length) {
    if (!present || packet == (const u8 *)0 || length == 0 || length > TX_BUFFER_SIZE) return 0;
    u32 index = tx_index++ & 3;
    copy_bytes(tx_buffers[index], packet, length);
    outl(io_base + TXSTATUS0 + index * 4, ((u32)length & 0x1fff) | TX_DMA | TX_RETRY);
    tx_count++;
    return length;
}
rtl_u32 rtl8139_rx(u8 *packet, u32 capacity) {
    if (!present || packet == (u8 *)0 || capacity == 0 || (inb(io_base + CHIPCMD) & 1) != 0) return 0;
    u16 status = (u16)rx_buffer[rx_offset] | ((u16)rx_buffer[rx_offset + 1] << 8);
    u16 length = (u16)rx_buffer[rx_offset + 2] |
        ((u16)rx_buffer[rx_offset + 3] << 8);
    if ((status & RX_OK) == 0 || length < 4 || length > RX_BUFFER_SIZE) return 0;
    u32 payload = length - 4;
    if (payload > capacity) payload = capacity;
    copy_bytes(packet, rx_buffer + rx_offset + 4, payload);
    rx_offset = (u16)((rx_offset + length + 4 + 3) & ~3);
    rx_offset %= RX_BUFFER_SIZE;
    outw(io_base + RXBUFPTR, (u16)(rx_offset - 16));
    rx_count++;
    return payload;
}
rtl_u32 rtl8139_rx_packets(void) { return rx_count; }
void rtl8139_interrupt(void) {
    if (!present) return;
    u16 status = inw(io_base + INTRSTATUS);
    if (status == 0) return;
    outw(io_base + INTRSTATUS, status);
    interrupt_count++;
    if (status & (IMR_RX_OK | IMR_RX_ERR)) rx_count++;
}
