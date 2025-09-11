#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "network.h"
#include "ip.h"

// ICMP types
#define ICMP_ECHO_REPLY 0
#define ICMP_DEST_UNREACHABLE 3
#define ICMP_SOURCE_QUENCH 4
#define ICMP_REDIRECT 5
#define ICMP_ECHO_REQUEST 8
#define ICMP_TIME_EXCEEDED 11
#define ICMP_PARAM_PROBLEM 12
#define ICMP_TIMESTAMP_REQUEST 13
#define ICMP_TIMESTAMP_REPLY 14

// ICMP codes for destination unreachable
#define ICMP_NET_UNREACHABLE 0
#define ICMP_HOST_UNREACHABLE 1
#define ICMP_PROTOCOL_UNREACHABLE 2
#define ICMP_PORT_UNREACHABLE 3
#define ICMP_FRAGMENTATION_NEEDED 4
#define ICMP_SOURCE_ROUTE_FAILED 5

// ICMP header
typedef struct __attribute__((packed)) icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    union {
        struct {
            uint16_t identifier;
            uint16_t sequence;
        } echo;
        struct {
            uint16_t unused;
            uint16_t mtu;
        } frag;
        uint32_t gateway;
    } data;
} icmp_header_t;

// ICMP packet structure
typedef struct icmp_packet {
    icmp_header_t header;
    uint8_t payload[1472]; // 1500 - 20 - 8 = 1472 max ICMP payload
} icmp_packet_t;

// Function prototypes
int icmp_init(void);
int icmp_send_echo_request(network_interface_t *iface, uint32_t dest_ip, uint16_t identifier, uint16_t sequence, void *data, size_t data_len);
int icmp_send_echo_reply(network_interface_t *iface, uint32_t dest_ip, uint16_t identifier, uint16_t sequence, void *data, size_t data_len);
int icmp_send_dest_unreachable(network_interface_t *iface, uint32_t dest_ip, uint8_t code, void *original_packet, size_t packet_len);
void icmp_process_packet(network_interface_t *iface, uint32_t src_ip, uint32_t dest_ip, icmp_packet_t *packet);
uint16_t icmp_checksum(icmp_header_t *header, size_t len);

#endif // ICMP_H
