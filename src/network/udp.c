#include "udp.h"
#include "ip.h"
#include "../memory/memory.h"

static udp_socket_t sockets[MAX_SOCKETS];
static int socket_count = 0;

int udp_init(void) {
    // Initialize sockets
    for (int i = 0; i < MAX_SOCKETS; i++) {
        sockets[i].local_port = 0;
        sockets[i].remote_ip = 0;
        sockets[i].remote_port = 0;
        sockets[i].bound = false;
        sockets[i].connected = false;
        sockets[i].receive_callback = NULL;
    }
    socket_count = 0;
    return NET_SUCCESS;
}

int udp_send_packet(network_interface_t *iface, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, void *payload, size_t payload_len) {
    if (!iface || !payload || payload_len == 0) {
        return NET_INVALID_PARAM;
    }

    if (payload_len > UDP_MAX_PAYLOAD) {
        return NET_ERROR;
    }

    udp_packet_t packet;
    
    // Build UDP header
    packet.header.src_port = __builtin_bswap16(src_port);
    packet.header.dest_port = __builtin_bswap16(dest_port);
    packet.header.length = __builtin_bswap16(UDP_HEADER_LEN + payload_len);
    packet.header.checksum = 0; // Optional for IPv4

    // Copy payload
    memcpy(packet.payload, payload, payload_len);

    // Calculate checksum (optional but recommended)
    packet.header.checksum = udp_checksum(&packet.header, iface->ip_address, dest_ip, UDP_HEADER_LEN + payload_len);

    // Send via IP layer
    return ip_send_packet(iface, dest_ip, IP_PROTOCOL_UDP, &packet, UDP_HEADER_LEN + payload_len);
}

void udp_process_packet(network_interface_t *iface, uint32_t src_ip, uint32_t dest_ip, udp_packet_t *packet) {
    if (!iface || !packet) {
        return;
    }

    // Convert ports from network byte order
    uint16_t src_port = __builtin_bswap16(packet->header.src_port);
    uint16_t dest_port = __builtin_bswap16(packet->header.dest_port);
    uint16_t length = __builtin_bswap16(packet->header.length);

    if (length < UDP_HEADER_LEN) {
        return; // Invalid packet
    }

    size_t payload_len = length - UDP_HEADER_LEN;

    // Find socket listening on destination port
    for (int i = 0; i < MAX_SOCKETS; i++) {
        udp_socket_t *socket = &sockets[i];
        if (socket->bound && socket->local_port == dest_port) {
            if (socket->receive_callback) {
                socket->receive_callback(socket, packet->payload, payload_len, src_ip, src_port);
            }
            break;
        }
    }
}

udp_socket_t *udp_create_socket(void) {
    if (socket_count >= MAX_SOCKETS) {
        return NULL;
    }

    udp_socket_t *socket = &sockets[socket_count];
    socket_count++;
    
    socket->local_port = 0;
    socket->remote_ip = 0;
    socket->remote_port = 0;
    socket->bound = false;
    socket->connected = false;
    socket->receive_callback = NULL;

    return socket;
}

int udp_bind(udp_socket_t *socket, uint16_t port) {
    if (!socket || port == 0) {
        return NET_INVALID_PARAM;
    }

    // Check if port is already in use
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].bound && sockets[i].local_port == port) {
            return NET_ERROR; // Port already in use
        }
    }

    socket->local_port = port;
    socket->bound = true;

    return NET_SUCCESS;
}

int udp_connect(udp_socket_t *socket, uint32_t remote_ip, uint16_t remote_port) {
    if (!socket || remote_ip == 0 || remote_port == 0) {
        return NET_INVALID_PARAM;
    }

    socket->remote_ip = remote_ip;
    socket->remote_port = remote_port;
    socket->connected = true;

    return NET_SUCCESS;
}

int udp_send(udp_socket_t *socket, void *data, size_t len) {
    if (!socket || !data || len == 0 || !socket->connected) {
        return NET_INVALID_PARAM;
    }

    return udp_sendto(socket, data, len, socket->remote_ip, socket->remote_port);
}

int udp_sendto(udp_socket_t *socket, void *data, size_t len, uint32_t dest_ip, uint16_t dest_port) {
    if (!socket || !data || len == 0 || dest_ip == 0 || dest_port == 0 || !socket->bound) {
        return NET_INVALID_PARAM;
    }

    // Find interface to use (simplified routing)
    network_interface_t *iface = network_find_route(dest_ip);
    if (!iface) {
        return NET_ERROR;
    }

    return udp_send_packet(iface, dest_ip, socket->local_port, dest_port, data, len);
}

void udp_close(udp_socket_t *socket) {
    if (socket) {
        socket->bound = false;
        socket->connected = false;
        socket->local_port = 0;
        socket->remote_ip = 0;
        socket->remote_port = 0;
        socket->receive_callback = NULL;
    }
}

uint16_t udp_checksum(udp_header_t *header, uint32_t src_ip, uint32_t dest_ip, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr;

    // Pseudo header
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dest_ip >> 16) & 0xFFFF;
    sum += dest_ip & 0xFFFF;
    sum += __builtin_bswap16(IP_PROTOCOL_UDP);
    sum += __builtin_bswap16(len);

    // UDP header and data
    ptr = (uint16_t *)header;
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
