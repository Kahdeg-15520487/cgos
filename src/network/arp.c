#include "arp.h"
#include "ethernet.h"
#include "../memory/memory.h"

static arp_entry_t arp_table[ARP_TABLE_SIZE];
static int arp_table_entries = 0;

// Simple timestamp counter (you might want to implement proper timing)
static uint32_t current_time = 0;

int arp_init(void) {
    // Initialize ARP table
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        arp_table[i].valid = false;
        arp_table[i].ip_address = 0;
        memset(arp_table[i].mac_address, 0, 6);
        arp_table[i].timestamp = 0;
    }
    arp_table_entries = 0;
    return NET_SUCCESS;
}

int arp_send_request(network_interface_t *iface, uint32_t target_ip) {
    if (!iface) {
        return NET_INVALID_PARAM;
    }

    arp_header_t arp_req;
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t zero_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Fill ARP request
    arp_req.hardware_type = __builtin_bswap16(ARP_HARDWARE_ETHERNET);
    arp_req.protocol_type = __builtin_bswap16(ARP_PROTOCOL_IP);
    arp_req.hardware_len = 6;
    arp_req.protocol_len = 4;
    arp_req.operation = __builtin_bswap16(ARP_REQUEST);
    
    memcpy(arp_req.sender_mac, iface->mac_address, 6);
    arp_req.sender_ip = __builtin_bswap32(iface->ip_address);
    memcpy(arp_req.target_mac, zero_mac, 6);
    arp_req.target_ip = __builtin_bswap32(target_ip);

    // Send ARP request as ethernet frame
    return ethernet_send_frame(iface, broadcast_mac, ETH_TYPE_ARP, &arp_req, sizeof(arp_req));
}

int arp_send_reply(network_interface_t *iface, uint32_t target_ip, uint8_t *target_mac) {
    if (!iface || !target_mac) {
        return NET_INVALID_PARAM;
    }

    arp_header_t arp_reply;

    // Fill ARP reply
    arp_reply.hardware_type = __builtin_bswap16(ARP_HARDWARE_ETHERNET);
    arp_reply.protocol_type = __builtin_bswap16(ARP_PROTOCOL_IP);
    arp_reply.hardware_len = 6;
    arp_reply.protocol_len = 4;
    arp_reply.operation = __builtin_bswap16(ARP_REPLY);
    
    memcpy(arp_reply.sender_mac, iface->mac_address, 6);
    arp_reply.sender_ip = __builtin_bswap32(iface->ip_address);
    memcpy(arp_reply.target_mac, target_mac, 6);
    arp_reply.target_ip = __builtin_bswap32(target_ip);

    // Send ARP reply as ethernet frame
    return ethernet_send_frame(iface, target_mac, ETH_TYPE_ARP, &arp_reply, sizeof(arp_reply));
}

void arp_process_packet(network_interface_t *iface, arp_header_t *arp_hdr) {
    if (!iface || !arp_hdr) {
        return;
    }

    // Convert from network byte order
    uint16_t operation = __builtin_bswap16(arp_hdr->operation);
    uint32_t sender_ip = __builtin_bswap32(arp_hdr->sender_ip);
    uint32_t target_ip = __builtin_bswap32(arp_hdr->target_ip);

    // Update ARP table with sender info
    arp_update_entry(sender_ip, arp_hdr->sender_mac);

    // Check if the request is for our IP
    if (target_ip == iface->ip_address) {
        if (operation == ARP_REQUEST) {
            // Send ARP reply
            arp_send_reply(iface, sender_ip, arp_hdr->sender_mac);
        }
    }
}

bool arp_lookup(uint32_t ip_address, uint8_t *mac_address) {
    if (!mac_address) {
        return false;
    }

    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip_address == ip_address) {
            memcpy(mac_address, arp_table[i].mac_address, 6);
            return true;
        }
    }
    return false;
}

int arp_add_entry(uint32_t ip_address, uint8_t *mac_address) {
    if (!mac_address) {
        return NET_INVALID_PARAM;
    }

    // Find empty slot or oldest entry
    int slot = -1;
    uint32_t oldest_time = current_time;
    
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!arp_table[i].valid) {
            slot = i;
            break;
        }
        if (arp_table[i].timestamp < oldest_time) {
            oldest_time = arp_table[i].timestamp;
            slot = i;
        }
    }

    if (slot == -1) {
        return NET_ERROR; // Table full
    }

    // Add entry
    arp_table[slot].ip_address = ip_address;
    memcpy(arp_table[slot].mac_address, mac_address, 6);
    arp_table[slot].timestamp = current_time++;
    arp_table[slot].valid = true;

    if (slot >= arp_table_entries) {
        arp_table_entries = slot + 1;
    }

    return NET_SUCCESS;
}

void arp_update_entry(uint32_t ip_address, uint8_t *mac_address) {
    if (!mac_address) {
        return;
    }

    // Check if entry exists
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip_address == ip_address) {
            memcpy(arp_table[i].mac_address, mac_address, 6);
            arp_table[i].timestamp = current_time++;
            return;
        }
    }

    // Entry doesn't exist, add it
    arp_add_entry(ip_address, mac_address);
}

void arp_print_table(void) {
    // This would print the ARP table - implement based on your graphics system
    // For now, just a placeholder
}
