#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "udp.h"
#include "tcp.h"
#include "icmp.h"
#include "../memory/memory.h"

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
    if (!arp_lookup(dest_ip, dest_mac)) {
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
        return; // Not IPv4
    }

    // Validate checksum
    if (!ip_validate_checksum(&packet->header)) {
        return; // Invalid checksum
    }

    // Convert addresses from network byte order
    uint32_t src_ip = __builtin_bswap32(packet->header.src_ip);
    uint32_t dest_ip = __builtin_bswap32(packet->header.dest_ip);

    // Check if packet is for us
    if (dest_ip != iface->ip_address) {
        return; // Not for us
    }

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
    // Simple implementation - would need proper parsing in real system
    // For now, return a placeholder
    return 0;
}

void ip_addr_to_str(uint32_t ip_addr, char *ip_str) {
    // Simple implementation - would need proper formatting in real system
    if (ip_str) {
        ip_str[0] = '\0';
    }
}

bool ip_is_local(network_interface_t *iface, uint32_t ip_addr) {
    if (!iface) {
        return false;
    }
    
    uint32_t network = iface->ip_address & iface->subnet_mask;
    uint32_t target_network = ip_addr & iface->subnet_mask;
    
    return (network == target_network);
}
