#include "fnet.h"
#include "rtl8139.h"

#define ETH_HEADER 14
#define ETH_ARP 0x0806
#define ARP_REQUEST 1
#define ARP_ETHERNET 1
#define ARP_IPV4 0x0800
#define FNET_FRAME_MAX 128

static fnet_u8 ready;
static fnet_u8 local_mac[6];
static fnet_u32 local_ip = 0x0f02000a;
static fnet_u32 gateway_ip = 0x0202000a;

static void put16(fnet_u8 *p, fnet_u16 value) { p[0] = (fnet_u8)(value >> 8); p[1] = (fnet_u8)value; }
static void put32(fnet_u8 *p, fnet_u32 value) {
    p[0] = (fnet_u8)(value >> 24); p[1] = (fnet_u8)(value >> 16);
    p[2] = (fnet_u8)(value >> 8); p[3] = (fnet_u8)value;
}
static fnet_u16 get16(const fnet_u8 *p) { return (fnet_u16)(((fnet_u16)p[0] << 8) | p[1]); }

fnet_u16 fnet_ipv4_checksum(const fnet_u8 *header, fnet_u32 length) {
    fnet_u32 sum = 0;
    for (fnet_u32 index = 0; index + 1 < length; index += 2) sum += get16(header + index);
    if ((length & 1) != 0) sum += (fnet_u16)header[length - 1] << 8;
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return (fnet_u16)~sum;
}

fnet_u8 fnet_init(void) {
    if (!rtl8139_is_present() || !rtl8139_link_up()) { ready = 0; return 0; }
    const fnet_u8 *mac = rtl8139_mac();
    for (fnet_u32 index = 0; index < 6; index++) local_mac[index] = mac[index];
    ready = 1;
    return 1;
}

fnet_u32 fnet_arp_probe(fnet_u32 target_ip) {
    fnet_u8 frame[FNET_FRAME_MAX];
    if (!ready) return 0;
    for (fnet_u32 index = 0; index < 6; index++) frame[index] = 0xff;
    for (fnet_u32 index = 0; index < 6; index++) frame[6 + index] = local_mac[index];
    put16(frame + 12, ETH_ARP);
    put16(frame + 14, ARP_ETHERNET);
    put16(frame + 16, ARP_IPV4);
    frame[18] = 6; frame[19] = 4;
    put16(frame + 20, ARP_REQUEST);
    for (fnet_u32 index = 0; index < 6; index++) frame[22 + index] = local_mac[index];
    put32(frame + 28, local_ip);
    for (fnet_u32 index = 0; index < 10; index++) frame[32 + index] = 0;
    put32(frame + 38, target_ip);
    for (fnet_u32 index = 42; index < 60; index++) frame[index] = 0;
    return rtl8139_tx(frame, 60);
}

fnet_u8 fnet_self_test(void) {
    fnet_u8 header[20];
    for (fnet_u32 index = 0; index < sizeof(header); index++) header[index] = 0;
    header[0] = 0x45; header[8] = 64; header[9] = 1;
    put16(header + 2, 20);
    put32(header + 12, local_ip);
    put32(header + 16, gateway_ip);
    put16(header + 10, 0);
    if (fnet_ipv4_checksum(header, sizeof(header)) == 0) return 0;
    return fnet_arp_probe(gateway_ip) == 60;
}

fnet_u8 fnet_is_ready(void) { return ready; }
