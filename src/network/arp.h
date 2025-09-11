#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "network.h"

// ARP constants
#define ARP_HARDWARE_ETHERNET 1
#define ARP_PROTOCOL_IP 0x0800
#define ARP_REQUEST 1
#define ARP_REPLY 2
#define ARP_TABLE_SIZE 128

// ARP header
typedef struct __attribute__((packed)) arp_header {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_len;
    uint8_t protocol_len;
    uint16_t operation;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} arp_header_t;

// ARP table entry
typedef struct arp_entry {
    uint32_t ip_address;
    uint8_t mac_address[6];
    uint32_t timestamp;
    bool valid;
} arp_entry_t;

// Function prototypes
int arp_init(void);
int arp_send_request(network_interface_t *iface, uint32_t target_ip);
int arp_send_reply(network_interface_t *iface, uint32_t target_ip, uint8_t *target_mac);
void arp_process_packet(network_interface_t *iface, arp_header_t *arp_hdr);
bool arp_lookup(uint32_t ip_address, uint8_t *mac_address);
int arp_add_entry(uint32_t ip_address, uint8_t *mac_address);
void arp_update_entry(uint32_t ip_address, uint8_t *mac_address);
void arp_print_table(void);

#endif // ARP_H
