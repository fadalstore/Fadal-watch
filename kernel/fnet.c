#include "fnet.h"
#include "rtl8139.h"

#define ETH_ARP 0x0806
#define ETH_IPV4 0x0800
#define IPV4_UDP 17
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_MAGIC 0x63825363
#define ARP_REPLY 2
#define ARP_ETHERNET 1
#define ARP_IPV4 0x0800
#define ARP_FRAME_LENGTH 42
#define FNET_FRAME_MAX 512
#define FNET_ARP_CACHE_SIZE 8
#define FNET_UDP_SOCKET_COUNT 8
#define FNET_UDP_PAYLOAD_MAX 128
#define FNET_IPV4_LOCAL 1
#define FNET_IPV4_NOT_LOCAL 0

static fnet_u8 ready;
static fnet_u8 local_mac[6];
static fnet_u32 local_ip = 0x0f02000a;
static fnet_u32 gateway_ip = 0x0202000a;
static fnet_u32 netmask = 0xffffff00;
static fnet_u32 dns_server_ip;
static fnet_u32 dhcp_xid = 0x46444c31;
static fnet_u8 dhcp_bound;
static fnet_u32 arp_cache_ip[FNET_ARP_CACHE_SIZE];
static fnet_u8 arp_cache_mac[FNET_ARP_CACHE_SIZE][6];
static fnet_u8 arp_cache_valid[FNET_ARP_CACHE_SIZE];
static fnet_u8 arp_cache_next;
static fnet_u32 ipv4_rx_count;
static fnet_u32 ipv4_drop_count;
struct fnet_udp_socket {
    fnet_u8 valid;
    fnet_u16 port;
    fnet_u32 source_ip;
    fnet_u16 source_port;
    fnet_u16 length;
    fnet_u8 payload[FNET_UDP_PAYLOAD_MAX];
};
static struct fnet_udp_socket udp_sockets[FNET_UDP_SOCKET_COUNT];

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
    dhcp_bound = 0;
    dns_server_ip = 0;
    for (fnet_u32 index = 0; index < FNET_UDP_SOCKET_COUNT; index++) {
        udp_sockets[index].valid = 0;
        udp_sockets[index].length = 0;
    }
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
    fnet_u8 dhcp_destination = header[9] == IPV4_UDP && total_length >= header_length + 8 &&
        get16(header + header_length + 2) == DHCP_CLIENT_PORT;
    if (version != 4 || header_length < 20 || total_length < header_length ||
        total_length > length || fnet_ipv4_checksum(header, header_length) != 0 ||
        (destination != local_ip && destination != 0xffffffff && !dhcp_destination)) {
        ipv4_drop_count++;
        return FNET_IPV4_NOT_LOCAL;
    }
    ipv4_rx_count++;
    return FNET_IPV4_LOCAL;
}
fnet_u8 fnet_ipv4_route(fnet_u32 destination, fnet_u32 *next_hop, fnet_u8 *mac) {
    fnet_u32 selected = (destination & netmask) == (local_ip & netmask) ?
        destination : gateway_ip;
    if (next_hop == (fnet_u32 *)0 || mac == (fnet_u8 *)0 || !ready) return 0;
    *next_hop = selected;
    if (fnet_arp_lookup(selected, mac)) return 1;
    fnet_arp_probe(selected);
    return 0;
}
static fnet_u16 udp_checksum(fnet_u32 source_ip, fnet_u32 destination_ip,
                             const fnet_u8 *udp, fnet_u32 length) {
    fnet_u8 pseudo[160];
    if (length > 140) return 0;
    put32(pseudo, source_ip); put32(pseudo + 4, destination_ip);
    pseudo[8] = 0; pseudo[9] = IPV4_UDP; put16(pseudo + 10, (fnet_u16)length);
    for (fnet_u32 index = 0; index < length; index++) pseudo[12 + index] = udp[index];
    return fnet_ipv4_checksum(pseudo, 12 + length);
}
fnet_u8 fnet_udp_bind(fnet_u16 port) {
    if (!ready || port == 0) return 0;
    for (fnet_u32 index = 0; index < FNET_UDP_SOCKET_COUNT; index++) {
        if (udp_sockets[index].valid && udp_sockets[index].port == port) return 1;
    }
    for (fnet_u32 index = 0; index < FNET_UDP_SOCKET_COUNT; index++) {
        if (!udp_sockets[index].valid) {
            udp_sockets[index].valid = 1;
            udp_sockets[index].port = port;
            udp_sockets[index].length = 0;
            return 1;
        }
    }
    return 0;
}
fnet_u16 fnet_udp_read(fnet_u16 port, fnet_u8 *payload, fnet_u16 capacity,
                       fnet_u32 *source_ip, fnet_u16 *source_port) {
    if (!payload || capacity == 0) return 0;
    for (fnet_u32 index = 0; index < FNET_UDP_SOCKET_COUNT; index++) {
        struct fnet_udp_socket *socket = &udp_sockets[index];
        if (!socket->valid || socket->port != port || socket->length == 0) continue;
        fnet_u16 length = socket->length < capacity ? socket->length : capacity;
        for (fnet_u32 byte = 0; byte < length; byte++) payload[byte] = socket->payload[byte];
        if (source_ip) *source_ip = socket->source_ip;
        if (source_port) *source_port = socket->source_port;
        socket->length = 0;
        return length;
    }
    return 0;
}
fnet_u8 fnet_udp_receive(const fnet_u8 *frame, fnet_u32 length) {
    if (!ready || !frame || length < 42 || get16(frame + 12) != ETH_IPV4) return 0;
    const fnet_u8 *ip = frame + 14;
    fnet_u32 header_length = (fnet_u32)(ip[0] & 0x0f) * 4;
    fnet_u32 total_length = get16(ip + 2);
    if ((ip[0] >> 4) != 4 || ip[9] != IPV4_UDP || header_length < 20 ||
        total_length < header_length + 8 || total_length > length) return 0;
    const fnet_u8 *udp = ip + header_length;
    fnet_u16 udp_length = get16(udp + 4);
    fnet_u16 checksum = get16(udp + 6);
    if (udp_length < 8 || udp_length > total_length - header_length ||
        (checksum != 0 && udp_checksum(get32(ip + 12), get32(ip + 16), udp, udp_length) != 0)) return 0;
    fnet_u16 destination_port = get16(udp + 2);
    for (fnet_u32 index = 0; index < FNET_UDP_SOCKET_COUNT; index++) {
        struct fnet_udp_socket *socket = &udp_sockets[index];
        if (!socket->valid || socket->port != destination_port || socket->length != 0) continue;
        socket->source_ip = get32(ip + 12);
        socket->source_port = get16(udp);
        socket->length = (fnet_u16)(udp_length - 8);
        if (socket->length > FNET_UDP_PAYLOAD_MAX) socket->length = FNET_UDP_PAYLOAD_MAX;
        for (fnet_u32 byte = 0; byte < socket->length; byte++) socket->payload[byte] = udp[8 + byte];
        return 1;
    }
    return 0;
}
fnet_u32 fnet_udp_send(fnet_u32 destination_ip, fnet_u16 source_port,
                       fnet_u16 destination_port, const fnet_u8 *payload,
                       fnet_u16 payload_length) {
    fnet_u8 frame[FNET_FRAME_MAX], mac[6];
    fnet_u32 next_hop;
    fnet_u32 udp_length = (fnet_u32)payload_length + 8;
    if (!ready || !payload || source_port == 0 || destination_port == 0 ||
        payload_length > FNET_UDP_PAYLOAD_MAX || !fnet_ipv4_route(destination_ip, &next_hop, mac)) return 0;
    copy_mac(frame, mac); copy_mac(frame + 6, local_mac); put16(frame + 12, ETH_IPV4);
    frame[14] = 0x45; frame[15] = 0; put16(frame + 16, (fnet_u16)(20 + udp_length));
    put16(frame + 18, 0); frame[20] = 64; frame[21] = IPV4_UDP; put16(frame + 24, 0);
    put32(frame + 26, local_ip); put32(frame + 30, destination_ip);
    put16(frame + 24, fnet_ipv4_checksum(frame + 14, 20));
    put16(frame + 34, source_port); put16(frame + 36, destination_port);
    put16(frame + 38, (fnet_u16)udp_length); put16(frame + 40, 0);
    for (fnet_u32 byte = 0; byte < payload_length; byte++) frame[42 + byte] = payload[byte];
    put16(frame + 40, udp_checksum(local_ip, destination_ip, frame + 34, udp_length));
    fnet_u32 wire_length = 14 + 20 + udp_length;
    if (wire_length < 60) wire_length = 60;
    for (fnet_u32 byte = 14 + 20 + udp_length; byte < wire_length; byte++) frame[byte] = 0;
    return rtl8139_tx(frame, wire_length);
}
fnet_u32 fnet_dhcp_discover(void) {
    fnet_u8 frame[FNET_FRAME_MAX];
    fnet_u8 *ip = frame + 14;
    fnet_u8 *udp = frame + 34;
    fnet_u8 *dhcp = frame + 42;
    fnet_u32 dhcp_length = 251;
    if (!ready) return 0;
    for (fnet_u32 index = 0; index < sizeof(frame); index++) frame[index] = 0;
    for (fnet_u32 index = 0; index < 6; index++) frame[index] = 0xff;
    copy_mac(frame + 6, local_mac); put16(frame + 12, ETH_IPV4);
    ip[0] = 0x45; put16(ip + 2, (fnet_u16)(20 + 8 + dhcp_length)); ip[8] = 64; ip[9] = IPV4_UDP;
    put32(ip + 12, 0); put32(ip + 16, 0xffffffff); put16(ip + 10, 0);
    put16(ip + 10, fnet_ipv4_checksum(ip, 20));
    put16(udp, DHCP_CLIENT_PORT); put16(udp + 2, DHCP_SERVER_PORT); put16(udp + 4, (fnet_u16)(8 + dhcp_length)); put16(udp + 6, 0);
    dhcp[0] = 1; dhcp[1] = 1; dhcp[2] = 6; put32(dhcp + 4, dhcp_xid); put16(dhcp + 10, 0x8000);
    copy_mac(dhcp + 28, local_mac); put32(dhcp + 236, DHCP_MAGIC);
    dhcp[240] = 53; dhcp[241] = 1; dhcp[242] = 1;
    dhcp[243] = 55; dhcp[244] = 3; dhcp[245] = 1; dhcp[246] = 3; dhcp[247] = 6;
    dhcp[248] = 255;
    return rtl8139_tx(frame, 14 + 20 + 8 + dhcp_length);
}
fnet_u8 fnet_dhcp_receive(const fnet_u8 *frame, fnet_u32 length) {
    if (!ready || !frame || length < 42 || get16(frame + 12) != ETH_IPV4) return 0;
    const fnet_u8 *ip = frame + 14;
    fnet_u32 header_length = (fnet_u32)(ip[0] & 0x0f) * 4;
    if ((ip[0] >> 4) != 4 || ip[9] != IPV4_UDP || header_length < 20) return 0;
    const fnet_u8 *udp = ip + header_length;
    if (get16(udp) != DHCP_SERVER_PORT || get16(udp + 2) != DHCP_CLIENT_PORT) return 0;
    fnet_u16 udp_length = get16(udp + 4);
    if (udp_length < 8 + 240 || 14 + header_length + udp_length > length) return 0;
    const fnet_u8 *dhcp = udp + 8;
    if (dhcp[0] != 2 || get32(dhcp + 4) != dhcp_xid || get32(dhcp + 236) != DHCP_MAGIC) return 0;
    local_ip = get32(dhcp + 16);
    fnet_u32 option = 240;
    fnet_u32 dhcp_length = (fnet_u32)udp_length - 8;
    while (option < dhcp_length) {
        fnet_u8 kind = dhcp[option++];
        if (kind == 255) break;
        if (kind == 0) continue;
        if (option >= dhcp_length) break;
        fnet_u8 option_length = dhcp[option++];
        if (option + option_length > dhcp_length) break;
        if (kind == 1 && option_length == 4) netmask = get32(dhcp + option);
        if (kind == 3 && option_length >= 4) gateway_ip = get32(dhcp + option);
        if (kind == 6 && option_length >= 4) dns_server_ip = get32(dhcp + option);
        option += option_length;
    }
    dhcp_bound = local_ip != 0;
    return dhcp_bound;
}
fnet_u8 fnet_dhcp_is_bound(void) { return dhcp_bound; }
fnet_u32 fnet_dhcp_dns_server(void) { return dns_server_ip; }
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
            if (frame[14 + 9] == IPV4_UDP) {
                if (!fnet_dhcp_receive(frame, length)) fnet_udp_receive(frame, length);
            }
            parsed++;
        }
    }
    return parsed;
}
fnet_u8 fnet_self_test(void) {
    fnet_u8 header[20], reply[ARP_FRAME_LENGTH], cached_mac[6], ipv4[64], udp_readback[8];
    fnet_u32 next_hop, source_ip;
    fnet_u16 source_port;
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
    if (!fnet_udp_bind(5353)) return 0;
    put16(ipv4 + 16, 31);
    ipv4[23] = IPV4_UDP;
    put16(ipv4 + 34, 4000); put16(ipv4 + 36, 5353); put16(ipv4 + 38, 11); put16(ipv4 + 40, 0);
    ipv4[42] = 'd'; ipv4[43] = 'n'; ipv4[44] = 's';
    put16(ipv4 + 24, 0);
    put16(ipv4 + 24, fnet_ipv4_checksum(ipv4 + 14, 20));
    put16(ipv4 + 40, udp_checksum(0x02020014, local_ip, ipv4 + 34, 11));
    if (!fnet_udp_receive(ipv4, 45) ||
        fnet_udp_read(5353, udp_readback, sizeof(udp_readback), &source_ip, &source_port) != 3 ||
        source_ip != 0x02020014 || source_port != 4000 || udp_readback[0] != 'd' ||
        udp_readback[1] != 'n' || udp_readback[2] != 's') return 0;
    if (!fnet_ipv4_route(0x08080808, &next_hop, cached_mac) || next_hop != gateway_ip) return 0;
    return fnet_arp_probe(gateway_ip) == 60;
}
fnet_u8 fnet_is_ready(void) { return ready; }
