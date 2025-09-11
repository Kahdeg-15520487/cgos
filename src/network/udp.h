#ifndef UDP_H
#define UDP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "network.h"
#include "ip.h"

// UDP constants
#define UDP_HEADER_LEN 8
#define UDP_MAX_PAYLOAD (IP_PACKET_SIZE - IP_HEADER_LEN - UDP_HEADER_LEN)

// UDP header
typedef struct __attribute__((packed)) udp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

// UDP packet structure
typedef struct udp_packet {
    udp_header_t header;
    uint8_t payload[UDP_MAX_PAYLOAD];
} udp_packet_t;

// UDP socket structure
typedef struct udp_socket {
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    bool bound;
    bool connected;
    void (*receive_callback)(struct udp_socket *socket, void *data, size_t len, uint32_t src_ip, uint16_t src_port);
} udp_socket_t;

// Function prototypes
int udp_init(void);
int udp_send_packet(network_interface_t *iface, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, void *payload, size_t payload_len);
void udp_process_packet(network_interface_t *iface, uint32_t src_ip, uint32_t dest_ip, udp_packet_t *packet);
udp_socket_t *udp_create_socket(void);
int udp_bind(udp_socket_t *socket, uint16_t port);
int udp_connect(udp_socket_t *socket, uint32_t remote_ip, uint16_t remote_port);
int udp_send(udp_socket_t *socket, void *data, size_t len);
int udp_sendto(udp_socket_t *socket, void *data, size_t len, uint32_t dest_ip, uint16_t dest_port);
void udp_close(udp_socket_t *socket);
uint16_t udp_checksum(udp_header_t *header, uint32_t src_ip, uint32_t dest_ip, size_t len);

#endif // UDP_H
