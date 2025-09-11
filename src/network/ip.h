#ifndef IP_H
#define IP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "network.h"

// IP constants
#define IP_VERSION_4 4
#define IP_HEADER_LEN 20
#define IP_PROTOCOL_ICMP 1
#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17
#define IP_FRAGMENT_SIZE 1480

// IP flags
#define IP_FLAG_DONT_FRAGMENT 0x4000
#define IP_FLAG_MORE_FRAGMENTS 0x2000
#define IP_FRAGMENT_OFFSET_MASK 0x1FFF

// IP header
typedef struct __attribute__((packed)) ip_header {
    uint8_t version_ihl;      // Version (4 bits) + IHL (4 bits)
    uint8_t tos;              // Type of Service
    uint16_t total_length;    // Total Length
    uint16_t identification;  // Identification
    uint16_t flags_fragment;  // Flags (3 bits) + Fragment Offset (13 bits)
    uint8_t ttl;              // Time to Live
    uint8_t protocol;         // Protocol
    uint16_t checksum;        // Header Checksum
    uint32_t src_ip;          // Source Address
    uint32_t dest_ip;         // Destination Address
} ip_header_t;

// IP packet structure
typedef struct ip_packet {
    ip_header_t header;
    uint8_t payload[IP_PACKET_SIZE - IP_HEADER_LEN];
} ip_packet_t;

// Function prototypes
int ip_init(void);
int ip_send_packet(network_interface_t *iface, uint32_t dest_ip, uint8_t protocol, void *payload, size_t payload_len);
void ip_process_packet(network_interface_t *iface, ip_packet_t *packet);
uint16_t ip_checksum(ip_header_t *header);
bool ip_validate_checksum(ip_header_t *header);
uint32_t ip_str_to_addr(const char *ip_str);
void ip_addr_to_str(uint32_t ip_addr, char *ip_str);
bool ip_is_local(network_interface_t *iface, uint32_t ip_addr);

#endif // IP_H
