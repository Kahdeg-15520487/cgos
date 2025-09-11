#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stddef.h>
#include "network.h"

// Ethernet frame structure
#define ETH_ALEN 6
#define ETH_HLEN 14
#define ETH_ZLEN 60
#define ETH_FRAME_LEN 1514

// Ethernet types
#define ETH_TYPE_IP 0x0800
#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV6 0x86DD

// Ethernet header
typedef struct __attribute__((packed)) ethernet_header {
    uint8_t dest_mac[ETH_ALEN];
    uint8_t src_mac[ETH_ALEN];
    uint16_t ethertype;
} ethernet_header_t;

// Ethernet frame
typedef struct ethernet_frame {
    ethernet_header_t header;
    uint8_t payload[ETH_FRAME_LEN - ETH_HLEN];
} ethernet_frame_t;

// Function prototypes
int ethernet_send_frame(network_interface_t *iface, uint8_t *dest_mac, uint16_t ethertype, void *payload, size_t payload_len);
int ethernet_receive_frame(network_interface_t *iface, ethernet_frame_t *frame);
void ethernet_process_frame(network_interface_t *iface, ethernet_frame_t *frame);
bool ethernet_is_broadcast(uint8_t *mac);
void ethernet_set_multicast(uint8_t *mac, uint32_t ip);

#endif // ETHERNET_H
