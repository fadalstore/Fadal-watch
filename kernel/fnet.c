#include "fnet.h"
#include "rtl8139.h"

#define ETH_ARP 0x0806
#define ETH_IPV4 0x0800
#define ARP_REPLY 2
#define ARP_ETHERNET 1
#define ARP_IPV4 0x0800
#define ARP_FRAME_LENGTH 42
#define FNET_FRAME_MAX 128
#define FNET_ARP_CACHE_SIZE 8
#define FNET_IPV4_LOCAL 1
#define FNET_IPV4_NOT_LOCAL 0

static fnet_u8 ready;
static fnet_u8 local_mac[6];
static fnet_u32 local_ip = 0x0f02000a;
static fnet_u32 gateway_ip = 0x0202000a;
static fnet_u32 arp_cache_ip[FNET_ARP_CACHE_SIZE];
static fnet_u8 arp_cache_mac[FNET_ARP_CACHE_SIZE][6];
static fnet_u8 arp_cache_valid[FNET_ARP_CACHE_SIZE];
static fnet_u8 arp_cache_next;
static fnet_u32 ipv4_rx_count;
static fnet_u32 ipv4_drop_count;

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
    ipv4_rx_count = 0;
    ipv4_drop_count = 0;
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
fnet_u8 fnet_ipv4_receive(const fnet_u8 *frame, fnet_u32 length) {
    if (!ready || frame == (const fnet_u8 *)0 || length < 34 ||
        get16(frame + 12) != ETH_IPV4) {
        ipv4_drop_count++;
        return FNET_IPV4_NOT_LOCAL;
    }
    const fnet_u8 *header = frame + 14;
    fnet_u8 version = (fnet_u8)(header[0] >> 4);
    fnet_u32 header_length = (fnet_u32)(header[0] & 0x0f) * 4;
    fnet_u32 total_length = get16(header + 2);
    fnet_u32 destination = get32(header + 16);
    if (version != 4 || header_length < 20 || total_length < header_length ||
        total_length > length || fnet_ipv4_checksum(header, header_length) != 0 ||
        (destination != local_ip && destination != 0xffffffff)) {
        ipv4_drop_count++;
        return FNET_IPV4_NOT_LOCAL;
    }
    ipv4_rx_count++;
    return FNET_IPV4_LOCAL;
}
fnet_u8 fnet_ipv4_route(fnet_u32 destination, fnet_u32 *next_hop, fnet_u8 *mac) {
    fnet_u32 selected = (destination & 0xffffff00) == (local_ip & 0xffffff00) ?
        destination : gateway_ip;
    if (next_hop == (fnet_u32 *)0 || mac == (fnet_u8 *)0 || !ready) return 0;
    *next_hop = selected;
    if (fnet_arp_lookup(selected, mac)) return 1;
    fnet_arp_probe(selected);
    return 0;
}
fnet_u32 fnet_poll(void) {
    fnet_u8 frame[FNET_FRAME_MAX];
    fnet_u32 parsed = 0;
    if (!ready) return 0;
    for (fnet_u32 attempt = 0; attempt < 4; attempt++) {
        fnet_u32 length = rtl8139_rx(frame, sizeof(frame));
        if (length == 0) break;
        if (get16(frame + 12) == ETH_ARP) {
            if (fnet_arp_receive(frame, length)) parsed++;
        } else if (fnet_ipv4_receive(frame, length)) {
            parsed++;
        }
    }
    return parsed;
}
fnet_u8 fnet_self_test(void) {
    fnet_u8 header[20], reply[ARP_FRAME_LENGTH], cached_mac[6], ipv4[34];
    fnet_u32 next_hop;
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
    for (fnet_u32 index = 0; index < sizeof(ipv4); index++) ipv4[index] = 0;
    copy_mac(ipv4, local_mac);
    put16(ipv4 + 12, 0x0800);
    ipv4[14] = 0x45;
    ipv4[22] = 64;
    ipv4[23] = 17;
    put16(ipv4 + 16, 20);
    put32(ipv4 + 26, 0x02020014);
    put32(ipv4 + 30, local_ip);
    put16(ipv4 + 24, 0);
    put16(ipv4 + 24, fnet_ipv4_checksum(ipv4 + 14, 20));
    if (fnet_ipv4_receive(ipv4, sizeof(ipv4)) != FNET_IPV4_LOCAL) return 0;
    if (!fnet_ipv4_route(0x08080808, &next_hop, cached_mac) || next_hop != gateway_ip) return 0;
    return fnet_arp_probe(gateway_ip) == 60;
}
fnet_u8 fnet_is_ready(void) { return ready; }
