#include "ethernet.h"
#include "arp.h"
#include "ip.h"
#include "../memory/memory.h"
#include "../debug/debug.h"

// Broadcast MAC address
static const uint8_t broadcast_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int ethernet_send_frame(network_interface_t *iface, uint8_t *dest_mac, uint16_t ethertype, void *payload, size_t payload_len) {
    if (!iface || !dest_mac || !payload || payload_len == 0) {
        return NET_INVALID_PARAM;
    }

    if (payload_len > (ETH_FRAME_LEN - ETH_HLEN)) {
        return NET_ERROR;
    }

    ethernet_frame_t frame;
    
    // Fill ethernet header
    memcpy(frame.header.dest_mac, dest_mac, ETH_ALEN);
    memcpy(frame.header.src_mac, iface->mac_address, ETH_ALEN);
    frame.header.ethertype = __builtin_bswap16(ethertype); // Convert to network byte order

    // Copy payload
    memcpy(frame.payload, payload, payload_len);

    // Pad frame if necessary
    size_t frame_len = ETH_HLEN + payload_len;
    if (frame_len < ETH_ZLEN) {
        memset(frame.payload + payload_len, 0, ETH_ZLEN - frame_len);
        frame_len = ETH_ZLEN;
    }

    // Send frame
    return network_send_raw(iface, &frame, frame_len);
}

int ethernet_receive_frame(network_interface_t *iface, ethernet_frame_t *frame) {
    if (!iface || !frame) {
        return NET_INVALID_PARAM;
    }

    int result = network_receive_raw(iface, frame, sizeof(ethernet_frame_t));
    if (result <= 0) {
        return result;
    }

    // Convert ethertype from network byte order
    frame->header.ethertype = __builtin_bswap16(frame->header.ethertype);

    return result;
}

void ethernet_process_frame(network_interface_t *iface, ethernet_frame_t *frame) {
    if (!iface || !frame) {
        return;
    }

    // Check if frame is for us or broadcast
    bool for_us = (memcmp(frame->header.dest_mac, iface->mac_address, ETH_ALEN) == 0);
    bool broadcast = ethernet_is_broadcast(frame->header.dest_mac);
    
    if (!for_us && !broadcast) {
        DEBUG_DEBUG("Ethernet: Frame not for us (dest MAC: %02x:%02x:%02x:%02x:%02x:%02x)\n",
                   frame->header.dest_mac[0], frame->header.dest_mac[1],
                   frame->header.dest_mac[2], frame->header.dest_mac[3],
                   frame->header.dest_mac[4], frame->header.dest_mac[5]);
        return; // Frame not for us
    }

    DEBUG_DEBUG("Ethernet: Processing frame (ethertype=0x%04x, %s)\n", 
               frame->header.ethertype, broadcast ? "broadcast" : "unicast");

    // Process based on ethertype
    switch (frame->header.ethertype) {
        case ETH_TYPE_ARP:
            DEBUG_DEBUG("Ethernet: Forwarding to ARP handler\n");
            arp_process_packet(iface, (arp_header_t *)frame->payload);
            break;
            
        case ETH_TYPE_IP:
            DEBUG_DEBUG("Ethernet: Forwarding to IP handler\n");
            ip_process_packet(iface, (ip_packet_t *)frame->payload);
            break;
            
        default:
            DEBUG_DEBUG("Ethernet: Unknown ethertype 0x%04x, ignoring\n", frame->header.ethertype);
            break;
    }
}

bool ethernet_is_broadcast(uint8_t *mac) {
    return (memcmp(mac, broadcast_mac, ETH_ALEN) == 0);
}

void ethernet_set_multicast(uint8_t *mac, uint32_t ip) {
    // Set multicast MAC based on IP (RFC 1112)
    mac[0] = 0x01;
    mac[1] = 0x00;
    mac[2] = 0x5E;
    mac[3] = (ip >> 16) & 0x7F;
    mac[4] = (ip >> 8) & 0xFF;
    mac[5] = ip & 0xFF;
}
