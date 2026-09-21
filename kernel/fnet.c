#include "fnet.h"
#include "rtl8139.h"

#define ETH_ARP 0x0806
#define ARP_REPLY 2
#define ARP_ETHERNET 1
#define ARP_IPV4 0x0800
#define ARP_FRAME_LENGTH 42
#define FNET_FRAME_MAX 128
#define FNET_ARP_CACHE_SIZE 8

static fnet_u8 ready;
static fnet_u8 local_mac[6];
static fnet_u32 local_ip = 0x0f02000a;
static fnet_u32 gateway_ip = 0x0202000a;
static fnet_u32 arp_cache_ip[FNET_ARP_CACHE_SIZE];
static fnet_u8 arp_cache_mac[FNET_ARP_CACHE_SIZE][6];
static fnet_u8 arp_cache_valid[FNET_ARP_CACHE_SIZE];
static fnet_u8 arp_cache_next;

static void put16(fnet_u8 *p, fnet_u16 value) { p[0] = (fnet_u8)(value >> 8); p[1] = (fnet_u8)value; }
static void put32(fnet_u8 *p, fnet_u32 value) {
    p[0] = (fnet_u8)(value >> 24); p[1] = (fnet_u8)(value >> 16);
    p[2] = (fnet_u8)(value >> 8); p[3] = (fnet_u8)value;
}
static fnet_u16 get16(const fnet_u8 *p) { return (fnet_u16)(((fnet_u16)p[0] << 8) | p[1]); }
static fnet_u32 get32(const fnet_u8 *p) {
    return ((fnet_u32)p[0] << 24) | ((fnet_u32)p[1] << 16) |
        ((fnet_u32)p[2] << 8) | p[3];
}
static void copy_mac(fnet_u8 *dst, const fnet_u8 *src) {
    for (fnet_u32 index = 0; index < 6; index++) dst[index] = src[index];
}
static void arp_cache_clear(void) {
    for (fnet_u32 index = 0; index < FNET_ARP_CACHE_SIZE; index++) {
        arp_cache_valid[index] = 0;
        arp_cache_ip[index] = 0;
    }
    arp_cache_next = 0;
}
static void arp_cache_store(fnet_u32 ip, const fnet_u8 *mac) {
    fnet_u32 slot = FNET_ARP_CACHE_SIZE;
    for (fnet_u32 index = 0; index < FNET_ARP_CACHE_SIZE; index++) {
        if (arp_cache_valid[index] && arp_cache_ip[index] == ip) { slot = index; break; }
    }
    if (slot == FNET_ARP_CACHE_SIZE) {
        slot = arp_cache_next;
        arp_cache_next = (fnet_u8)((arp_cache_next + 1) % FNET_ARP_CACHE_SIZE);
    }
    arp_cache_ip[slot] = ip;
    copy_mac(arp_cache_mac[slot], mac);
    arp_cache_valid[slot] = 1;
}

fnet_u16 fnet_ipv4_checksum(const fnet_u8 *header, fnet_u32 length) {
    fnet_u32 sum = 0;
    for (fnet_u32 index = 0; index + 1 < length; index += 2) sum += get16(header + index);
    if ((length & 1) != 0) sum += (fnet_u16)header[length - 1] << 8;
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return (fnet_u16)~sum;
}
fnet_u8 fnet_init(void) {
    if (!rtl8139_is_present() || !rtl8139_link_up()) { ready = 0; return 0; }
    copy_mac(local_mac, rtl8139_mac());
    arp_cache_clear();
    ready = 1;
    return 1;
}
fnet_u32 fnet_arp_probe(fnet_u32 target_ip) {
    fnet_u8 frame[FNET_FRAME_MAX];
    if (!ready) return 0;
    for (fnet_u32 index = 0; index < 6; index++) frame[index] = 0xff;
    copy_mac(frame + 6, local_mac);
    put16(frame + 12, ETH_ARP); put16(frame + 14, ARP_ETHERNET); put16(frame + 16, ARP_IPV4);
    frame[18] = 6; frame[19] = 4; put16(frame + 20, 1);
    copy_mac(frame + 22, local_mac); put32(frame + 28, local_ip);
    for (fnet_u32 index = 0; index < 10; index++) frame[32 + index] = 0;
    put32(frame + 38, target_ip);
    for (fnet_u32 index = 42; index < 60; index++) frame[index] = 0;
    return rtl8139_tx(frame, 60);
}
fnet_u8 fnet_arp_receive(const fnet_u8 *frame, fnet_u32 length) {
    if (!ready || frame == (const fnet_u8 *)0 || length < ARP_FRAME_LENGTH ||
        get16(frame + 12) != ETH_ARP || get16(frame + 14) != ARP_ETHERNET ||
        get16(frame + 16) != ARP_IPV4 || frame[18] != 6 || frame[19] != 4 ||
        get16(frame + 20) != ARP_REPLY) return 0;
    fnet_u32 sender_ip = get32(frame + 28);
    if (sender_ip == 0 || sender_ip == 0xffffffff) return 0;
    arp_cache_store(sender_ip, frame + 22);
    return 1;
}
fnet_u8 fnet_arp_lookup(fnet_u32 ip, fnet_u8 *mac) {
    if (!ready || mac == (fnet_u8 *)0) return 0;
    for (fnet_u32 index = 0; index < FNET_ARP_CACHE_SIZE; index++) {
        if (arp_cache_valid[index] && arp_cache_ip[index] == ip) {
            copy_mac(mac, arp_cache_mac[index]); return 1;
        }
    }
    return 0;
}
fnet_u32 fnet_poll(void) {
    fnet_u8 frame[FNET_FRAME_MAX];
    fnet_u32 parsed = 0;
    if (!ready) return 0;
    for (fnet_u32 attempt = 0; attempt < 4; attempt++) {
        fnet_u32 length = rtl8139_rx(frame, sizeof(frame));
        if (length == 0) break;
        if (fnet_arp_receive(frame, length)) parsed++;
    }
    return parsed;
}
fnet_u8 fnet_self_test(void) {
    fnet_u8 header[20], reply[ARP_FRAME_LENGTH], cached_mac[6];
    for (fnet_u32 index = 0; index < sizeof(header); index++) header[index] = 0;
    header[0] = 0x45; header[8] = 64; header[9] = 1; put16(header + 2, 20);
    put32(header + 12, local_ip); put32(header + 16, gateway_ip); put16(header + 10, 0);
    if (fnet_ipv4_checksum(header, sizeof(header)) == 0) return 0;
    for (fnet_u32 index = 0; index < sizeof(reply); index++) reply[index] = 0;
    copy_mac(reply, local_mac); put16(reply + 12, ETH_ARP); put16(reply + 14, ARP_ETHERNET);
    put16(reply + 16, ARP_IPV4); reply[18] = 6; reply[19] = 4; put16(reply + 20, ARP_REPLY);
    for (fnet_u32 index = 0; index < 6; index++) reply[22 + index] = (fnet_u8)(0x10 + index);
    put32(reply + 28, gateway_ip); copy_mac(reply + 32, local_mac); put32(reply + 38, local_ip);
    if (!fnet_arp_receive(reply, sizeof(reply)) || !fnet_arp_lookup(gateway_ip, cached_mac)) return 0;
    for (fnet_u32 index = 0; index < 6; index++) if (cached_mac[index] != (fnet_u8)(0x10 + index)) return 0;
    return fnet_arp_probe(gateway_ip) == 60;
}
fnet_u8 fnet_is_ready(void) { return ready; }
