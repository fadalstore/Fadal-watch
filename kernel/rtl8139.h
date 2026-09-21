#ifndef FADAL_RTL8139_H
#define FADAL_RTL8139_H

typedef unsigned char rtl_u8;
typedef unsigned short rtl_u16;
typedef unsigned int rtl_u32;

rtl_u8 rtl8139_probe(void);
rtl_u8 rtl8139_is_present(void);
rtl_u8 rtl8139_link_up(void);
rtl_u32 rtl8139_irq(void);
rtl_u32 rtl8139_tx(const rtl_u8 *packet, rtl_u32 length);
rtl_u32 rtl8139_rx(rtl_u8 *packet, rtl_u32 capacity);
rtl_u32 rtl8139_rx_packets(void);
const rtl_u8 *rtl8139_mac(void);
void rtl8139_interrupt(void);

#endif
