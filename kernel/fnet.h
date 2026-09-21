#ifndef FADAL_FNET_H
#define FADAL_FNET_H

typedef unsigned char fnet_u8;
typedef unsigned short fnet_u16;
typedef unsigned int fnet_u32;

fnet_u8 fnet_init(void);
fnet_u8 fnet_self_test(void);
fnet_u32 fnet_arp_probe(fnet_u32 target_ip);
fnet_u16 fnet_ipv4_checksum(const fnet_u8 *header, fnet_u32 length);
fnet_u8 fnet_is_ready(void);

#endif
