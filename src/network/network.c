#include "network.h"
#include "ethernet.h"
#include "arp.h"
#include "ip.h"
#include "udp.h"
#include "tcp.h"
#include "icmp.h"
#include "netdev.h"
#include "../memory/memory.h"
#include "../graphic/graphic.h"
#include "../debug/debug.h"

static network_interface_t *interfaces[MAX_NETWORK_INTERFACES];
static int interface_count = 0;

int network_init(void) {
    DEBUG_INFO("Initializing network subsystem\n");
    
    // Initialize all network interfaces to NULL
    for (int i = 0; i < MAX_NETWORK_INTERFACES; i++) {
        interfaces[i] = NULL;
    }
    interface_count = 0;

    // Initialize network protocols
    DEBUG_DEBUG("Initializing ARP protocol\n");
    if (arp_init() != NET_SUCCESS) {
        DEBUG_ERROR("Failed to initialize ARP protocol\n");
        return NET_ERROR;
    }
    
    DEBUG_DEBUG("Initializing IP protocol\n");
    if (ip_init() != NET_SUCCESS) {
        DEBUG_ERROR("Failed to initialize IP protocol\n");
        return NET_ERROR;
    }
    
    DEBUG_DEBUG("Initializing UDP protocol\n");
    if (udp_init() != NET_SUCCESS) {
        DEBUG_ERROR("Failed to initialize UDP protocol\n");
        return NET_ERROR;
    }
    
    DEBUG_DEBUG("Initializing TCP protocol\n");
    if (tcp_init() != NET_SUCCESS) {
        DEBUG_ERROR("Failed to initialize TCP protocol\n");
        return NET_ERROR;
    }
    
    DEBUG_DEBUG("Initializing ICMP protocol\n");
    if (icmp_init() != NET_SUCCESS) {
        DEBUG_ERROR("Failed to initialize ICMP protocol\n");
        return NET_ERROR;
    }

    // Initialize network devices
    DEBUG_DEBUG("Initializing loopback interface\n");
    if (loopback_init() != NET_SUCCESS) {
        DEBUG_ERROR("Failed to initialize loopback interface\n");
        return NET_ERROR;
    }
    
    // Initialize ethernet interface for DHCP demo
    DEBUG_DEBUG("Initializing ethernet interface\n");
    if (ethernet_init() != NET_SUCCESS) {
        DEBUG_WARN("Failed to initialize ethernet interface, continuing with loopback only\n");
        // Don't fail if ethernet init fails, just continue with loopback
    }

    DEBUG_INFO("Network subsystem initialization completed successfully\n");
    return NET_SUCCESS;
}

int network_register_interface(network_interface_t *iface) {
    if (interface_count >= MAX_NETWORK_INTERFACES || iface == NULL) {
        return NET_ERROR;
    }

    interfaces[interface_count] = iface;
    interface_count++;
    iface->active = true;

    return NET_SUCCESS;
}

network_interface_t *network_get_interface(int index) {
    if (index < 0 || index >= interface_count) {
        return NULL;
    }
    return interfaces[index];
}

int network_send_raw(network_interface_t *iface, void *data, size_t len) {
    if (iface == NULL || data == NULL || len == 0 || !iface->active) {
        return NET_INVALID_PARAM;
    }

    if (iface->send_packet) {
        iface->send_packet(iface, data, len);
        return NET_SUCCESS;
    }

    return NET_ERROR;
}

int network_receive_raw(network_interface_t *iface, void *buffer, size_t max_len) {
    if (iface == NULL || buffer == NULL || max_len == 0 || !iface->active) {
        return NET_INVALID_PARAM;
    }

    if (iface->receive_packet) {
        return iface->receive_packet(iface, buffer, max_len);
    }

    return NET_ERROR;
}

void network_process_packets(void) {
    ethernet_frame_t frame;
    
    // Process packets on all active interfaces
    for (int i = 0; i < interface_count; i++) {
        network_interface_t *iface = interfaces[i];
        if (!iface || !iface->active) {
            continue;
        }

        // Try to receive and process frames
        while (ethernet_receive_frame(iface, &frame) > 0) {
            ethernet_process_frame(iface, &frame);
        }
    }
}

// Utility function to find interface by IP
network_interface_t *network_find_interface_by_ip(uint32_t ip) {
    for (int i = 0; i < interface_count; i++) {
        if (interfaces[i] && interfaces[i]->active && interfaces[i]->ip_address == ip) {
            return interfaces[i];
        }
    }
    return NULL;
}

// Utility function to find best interface for destination
network_interface_t *network_find_route(uint32_t dest_ip) {
    // Simple routing: check if destination is on local subnet
    for (int i = 0; i < interface_count; i++) {
        network_interface_t *iface = interfaces[i];
        if (iface && iface->active) {
            uint32_t network = iface->ip_address & iface->subnet_mask;
            uint32_t dest_network = dest_ip & iface->subnet_mask;
            if (network == dest_network) {
                return iface;
            }
        }
    }
    
    // If not local, use first active interface (default route)
    for (int i = 0; i < interface_count; i++) {
        if (interfaces[i] && interfaces[i]->active) {
            return interfaces[i];
        }
    }
    
    return NULL;
}
