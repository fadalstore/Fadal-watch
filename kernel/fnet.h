#ifndef FADAL_FNET_H
#define FADAL_FNET_H

typedef unsigned char fnet_u8;
typedef unsigned short fnet_u16;
typedef unsigned int fnet_u32;

fnet_u8 fnet_init(void);
fnet_u8 fnet_self_test(void);
fnet_u32 fnet_arp_probe(fnet_u32 target_ip);
fnet_u8 fnet_arp_receive(const fnet_u8 *frame, fnet_u32 length);
fnet_u8 fnet_arp_lookup(fnet_u32 ip, fnet_u8 *mac);
fnet_u8 fnet_ipv4_receive(const fnet_u8 *frame, fnet_u32 length);
fnet_u8 fnet_ipv4_route(fnet_u32 destination, fnet_u32 *next_hop, fnet_u8 *mac);
fnet_u32 fnet_poll(void);
fnet_u16 fnet_ipv4_checksum(const fnet_u8 *header, fnet_u32 length);
fnet_u8 fnet_is_ready(void);

#endif
