#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "udp.h"
#include "tcp.h"
#include "icmp.h"
#include "../memory/memory.h"
#include "../debug/debug.h"

static uint16_t ip_identification = 1;

int ip_init(void) {
    ip_identification = 1;
    return NET_SUCCESS;
}

int ip_send_packet(network_interface_t *iface, uint32_t dest_ip, uint8_t protocol, void *payload, size_t payload_len) {
    if (!iface || !payload || payload_len == 0) {
        return NET_INVALID_PARAM;
    }

    if (payload_len > (IP_PACKET_SIZE - IP_HEADER_LEN)) {
        return NET_ERROR; // Packet too large
    }

    ip_packet_t packet;
    uint8_t dest_mac[6];

    // Build IP header
    packet.header.version_ihl = (IP_VERSION_4 << 4) | (IP_HEADER_LEN / 4);
    packet.header.tos = 0;
    packet.header.total_length = __builtin_bswap16(IP_HEADER_LEN + payload_len);
    packet.header.identification = __builtin_bswap16(ip_identification++);
    packet.header.flags_fragment = __builtin_bswap16(IP_FLAG_DONT_FRAGMENT);
    packet.header.ttl = 64;
    packet.header.protocol = protocol;
    packet.header.src_ip = __builtin_bswap32(iface->ip_address);
    packet.header.dest_ip = __builtin_bswap32(dest_ip);
    packet.header.checksum = 0;

    // Calculate checksum
    packet.header.checksum = ip_checksum(&packet.header);

    // Copy payload
    memcpy(packet.payload, payload, payload_len);

    // Resolve destination MAC address
    // For broadcast address, use broadcast MAC directly
    if (dest_ip == 0xFFFFFFFF) {
        // Broadcast - use FF:FF:FF:FF:FF:FF
        dest_mac[0] = 0xFF;
        dest_mac[1] = 0xFF;
        dest_mac[2] = 0xFF;
        dest_mac[3] = 0xFF;
        dest_mac[4] = 0xFF;
        dest_mac[5] = 0xFF;
    } else if (!arp_lookup(dest_ip, dest_mac)) {
        // MAC not in ARP table, send ARP request
        arp_send_request(iface, dest_ip);
        return NET_TIMEOUT; // Would need to queue packet in real implementation
    }

    // Send IP packet as ethernet frame
    return ethernet_send_frame(iface, dest_mac, ETH_TYPE_IP, &packet, IP_HEADER_LEN + payload_len);
}

void ip_process_packet(network_interface_t *iface, ip_packet_t *packet) {
    if (!iface || !packet) {
        return;
    }

    // Validate IP header
    uint8_t version = (packet->header.version_ihl >> 4) & 0x0F;
    if (version != IP_VERSION_4) {
        DEBUG_WARN("IP: Not IPv4 (version=%d)\n", version);
        return; // Not IPv4
    }

    // Validate checksum
    if (!ip_validate_checksum(&packet->header)) {
        DEBUG_WARN("IP: Invalid checksum\n");
        return; // Invalid checksum
    }

    // Convert addresses from network byte order
    uint32_t src_ip = __builtin_bswap32(packet->header.src_ip);
    uint32_t dest_ip = __builtin_bswap32(packet->header.dest_ip);

    // Check if packet is for us
    // Accept if:
    // 1. Destination matches our IP
    // 2. Destination is broadcast (255.255.255.255)
    // 3. We have no IP yet (0.0.0.0) - needed for DHCP
    // 4. Destination is subnet broadcast
    bool is_broadcast = (dest_ip == 0xFFFFFFFF);
    bool is_for_us = (dest_ip == iface->ip_address);
    bool waiting_for_dhcp = (iface->ip_address == 0);
    bool is_subnet_broadcast = (iface->subnet_mask != 0 && 
                                 (dest_ip | iface->subnet_mask) == 0xFFFFFFFF);
    
    if (!is_for_us && !is_broadcast && !waiting_for_dhcp && !is_subnet_broadcast) {
        DEBUG_DEBUG("IP: Packet not for us (dest=%d.%d.%d.%d, our_ip=%d.%d.%d.%d)\n",
                   (dest_ip >> 24) & 0xFF, (dest_ip >> 16) & 0xFF,
                   (dest_ip >> 8) & 0xFF, dest_ip & 0xFF,
                   (iface->ip_address >> 24) & 0xFF, (iface->ip_address >> 16) & 0xFF,
                   (iface->ip_address >> 8) & 0xFF, iface->ip_address & 0xFF);
        return; // Not for us
    }

    DEBUG_DEBUG("IP: Processing packet (proto=%d, src=%d.%d.%d.%d, dest=%d.%d.%d.%d)\n",
               packet->header.protocol,
               (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
               (src_ip >> 8) & 0xFF, src_ip & 0xFF,
               (dest_ip >> 24) & 0xFF, (dest_ip >> 16) & 0xFF,
               (dest_ip >> 8) & 0xFF, dest_ip & 0xFF);

    // Process based on protocol
    switch (packet->header.protocol) {
        case IP_PROTOCOL_ICMP:
            icmp_process_packet(iface, src_ip, dest_ip, (icmp_packet_t *)packet->payload);
            break;
            
        case IP_PROTOCOL_UDP:
            udp_process_packet(iface, src_ip, dest_ip, (udp_packet_t *)packet->payload);
            break;
            
        case IP_PROTOCOL_TCP:
            tcp_process_packet(iface, src_ip, dest_ip, (tcp_packet_t *)packet->payload);
            break;
            
        default:
            // Unknown protocol, send ICMP protocol unreachable
            icmp_send_dest_unreachable(iface, src_ip, ICMP_PROTOCOL_UNREACHABLE, packet, sizeof(ip_header_t) + 8);
            break;
    }
}

uint16_t ip_checksum(ip_header_t *header) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)header;

    // Sum all 16-bit words in header
    for (int i = 0; i < IP_HEADER_LEN / 2; i++) {
        sum += ptr[i];
    }

    // Add carry
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // One's complement
    return ~sum;
}

bool ip_validate_checksum(ip_header_t *header) {
    uint16_t original_checksum = header->checksum;
    header->checksum = 0;
    uint16_t calculated_checksum = ip_checksum(header);
    header->checksum = original_checksum;
    
    return (original_checksum == calculated_checksum);
}

uint32_t ip_str_to_addr(const char *ip_str) {
    if (!ip_str) {
        return 0;
    }
    
    uint32_t octets[4] = {0};
    int octet_idx = 0;
    uint32_t current = 0;
    
    for (const char *p = ip_str; *p != '\0' && octet_idx < 4; p++) {
        if (*p >= '0' && *p <= '9') {
            current = current * 10 + (*p - '0');
            if (current > 255) {
                return 0; // Invalid octet value
            }
        } else if (*p == '.') {
            octets[octet_idx++] = current;
            current = 0;
        } else {
            return 0; // Invalid character
        }
    }
    
    // Store last octet
    if (octet_idx < 4) {
        octets[octet_idx] = current;
    }
    
    // Must have exactly 4 octets
    if (octet_idx != 3) {
        return 0;
    }
    
    // Combine into 32-bit address (host byte order)
    return (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
}

void ip_addr_to_str(uint32_t ip_addr, char *ip_str) {
    if (!ip_str) {
        return;
    }
    
    uint8_t octets[4];
    octets[0] = (ip_addr >> 24) & 0xFF;
    octets[1] = (ip_addr >> 16) & 0xFF;
    octets[2] = (ip_addr >> 8) & 0xFF;
    octets[3] = ip_addr & 0xFF;
    
    // Format as "xxx.xxx.xxx.xxx"
    char *p = ip_str;
    for (int i = 0; i < 4; i++) {
        uint8_t val = octets[i];
        
        // Write digits
        if (val >= 100) {
            *p++ = '0' + (val / 100);
            val %= 100;
            *p++ = '0' + (val / 10);
            *p++ = '0' + (val % 10);
        } else if (val >= 10) {
            *p++ = '0' + (val / 10);
            *p++ = '0' + (val % 10);
        } else {
            *p++ = '0' + val;
        }
        
        // Add dot separator (except for last octet)
        if (i < 3) {
            *p++ = '.';
        }
    }
    *p = '\0';
}

bool ip_is_local(network_interface_t *iface, uint32_t ip_addr) {
    if (!iface) {
        return false;
    }
    
    uint32_t network = iface->ip_address & iface->subnet_mask;
    uint32_t target_network = ip_addr & iface->subnet_mask;
    
    return (network == target_network);
}
