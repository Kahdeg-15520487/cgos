#include "icmp.h"
#include "ip.h"
#include "../memory/memory.h"

int icmp_init(void) {
    return NET_SUCCESS;
}

int icmp_send_echo_request(network_interface_t *iface, uint32_t dest_ip, uint16_t identifier, uint16_t sequence, void *data, size_t data_len) {
    if (!iface) {
        return NET_INVALID_PARAM;
    }

    icmp_packet_t packet;
    
    // Build ICMP header
    packet.header.type = ICMP_ECHO_REQUEST;
    packet.header.code = 0;
    packet.header.checksum = 0;
    packet.header.data.echo.identifier = __builtin_bswap16(identifier);
    packet.header.data.echo.sequence = __builtin_bswap16(sequence);

    // Copy data if provided
    size_t payload_len = 0;
    if (data && data_len > 0) {
        size_t max_data = sizeof(packet.payload);
        payload_len = (data_len > max_data) ? max_data : data_len;
        memcpy(packet.payload, data, payload_len);
    }

    // Calculate checksum
    packet.header.checksum = icmp_checksum(&packet.header, sizeof(icmp_header_t) + payload_len);

    // Send via IP layer
    return ip_send_packet(iface, dest_ip, IP_PROTOCOL_ICMP, &packet, sizeof(icmp_header_t) + payload_len);
}

int icmp_send_echo_reply(network_interface_t *iface, uint32_t dest_ip, uint16_t identifier, uint16_t sequence, void *data, size_t data_len) {
    if (!iface) {
        return NET_INVALID_PARAM;
    }

    icmp_packet_t packet;
    
    // Build ICMP header
    packet.header.type = ICMP_ECHO_REPLY;
    packet.header.code = 0;
    packet.header.checksum = 0;
    packet.header.data.echo.identifier = __builtin_bswap16(identifier);
    packet.header.data.echo.sequence = __builtin_bswap16(sequence);

    // Copy data if provided
    size_t payload_len = 0;
    if (data && data_len > 0) {
        size_t max_data = sizeof(packet.payload);
        payload_len = (data_len > max_data) ? max_data : data_len;
        memcpy(packet.payload, data, payload_len);
    }

    // Calculate checksum
    packet.header.checksum = icmp_checksum(&packet.header, sizeof(icmp_header_t) + payload_len);

    // Send via IP layer
    return ip_send_packet(iface, dest_ip, IP_PROTOCOL_ICMP, &packet, sizeof(icmp_header_t) + payload_len);
}

int icmp_send_dest_unreachable(network_interface_t *iface, uint32_t dest_ip, uint8_t code, void *original_packet, size_t packet_len) {
    if (!iface || !original_packet) {
        return NET_INVALID_PARAM;
    }

    icmp_packet_t packet;
    
    // Build ICMP header
    packet.header.type = ICMP_DEST_UNREACHABLE;
    packet.header.code = code;
    packet.header.checksum = 0;
    packet.header.data.gateway = 0; // Unused for destination unreachable

    // Copy original packet data (IP header + 8 bytes of data)
    size_t copy_len = (packet_len > sizeof(packet.payload)) ? sizeof(packet.payload) : packet_len;
    memcpy(packet.payload, original_packet, copy_len);

    // Calculate checksum
    packet.header.checksum = icmp_checksum(&packet.header, sizeof(icmp_header_t) + copy_len);

    // Send via IP layer
    return ip_send_packet(iface, dest_ip, IP_PROTOCOL_ICMP, &packet, sizeof(icmp_header_t) + copy_len);
}

void icmp_process_packet(network_interface_t *iface, uint32_t src_ip, uint32_t dest_ip, icmp_packet_t *packet) {
    if (!iface || !packet) {
        return;
    }

    // Validate checksum
    uint16_t original_checksum = packet->header.checksum;
    packet->header.checksum = 0;
    uint16_t calculated_checksum = icmp_checksum(&packet->header, sizeof(icmp_header_t));
    
    if (original_checksum != calculated_checksum) {
        return; // Invalid checksum
    }

    switch (packet->header.type) {
        case ICMP_ECHO_REQUEST:
            // Respond with echo reply
            {
                uint16_t identifier = __builtin_bswap16(packet->header.data.echo.identifier);
                uint16_t sequence = __builtin_bswap16(packet->header.data.echo.sequence);
                icmp_send_echo_reply(iface, src_ip, identifier, sequence, packet->payload, 0);
            }
            break;
            
        case ICMP_ECHO_REPLY:
            // Handle ping reply (could notify waiting applications)
            break;
            
        case ICMP_DEST_UNREACHABLE:
            // Handle destination unreachable (could notify transport layer)
            break;
            
        default:
            // Unknown ICMP type, ignore
            break;
    }
}

uint16_t icmp_checksum(icmp_header_t *header, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)header;

    // Sum all 16-bit words
    for (size_t i = 0; i < len / 2; i++) {
        sum += ptr[i];
    }

    // Handle odd byte
    if (len & 1) {
        sum += ((uint8_t *)header)[len - 1] << 8;
    }

    // Add carry
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}
