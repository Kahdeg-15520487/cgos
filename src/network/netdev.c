#include "netdev.h"
#include "network.h"
#include "../memory/memory.h"
#include "../drivers/e1000.h"
#include "../pci/pci.h"
#include "../debug/debug.h"

// Simple string functions for kernel
static int netdev_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static char *netdev_strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

#define MAX_PACKET_QUEUE 16

// Simple packet queue for loopback
typedef struct packet_queue {
    uint8_t packets[MAX_PACKET_QUEUE][ETHERNET_FRAME_SIZE];
    size_t lengths[MAX_PACKET_QUEUE];
    int head;
    int tail;
    int count;
} packet_queue_t;

static packet_queue_t loopback_queue = {0};

// Loopback device operations
static int loopback_send(network_interface_t *iface, void *data, size_t len) {
    (void)iface; // Unused - loopback uses global queue
    
    if (!data || len == 0 || len > ETHERNET_FRAME_SIZE) {
        return NET_INVALID_PARAM;
    }

    // Add packet to queue
    if (loopback_queue.count >= MAX_PACKET_QUEUE) {
        return NET_BUFFER_FULL;
    }

    int index = loopback_queue.tail;
    memcpy(loopback_queue.packets[index], data, len);
    loopback_queue.lengths[index] = len;
    loopback_queue.tail = (loopback_queue.tail + 1) % MAX_PACKET_QUEUE;
    loopback_queue.count++;

    return NET_SUCCESS;
}

static int loopback_receive(network_interface_t *iface, void *buffer, size_t max_len) {
    (void)iface; // Unused - loopback uses global queue
    
    if (!buffer || max_len == 0) {
        return NET_INVALID_PARAM;
    }

    // Get packet from queue
    if (loopback_queue.count == 0) {
        return 0; // No packets available
    }

    int index = loopback_queue.head;
    size_t len = loopback_queue.lengths[index];
    
    if (len > max_len) {
        return NET_ERROR; // Buffer too small
    }

    memcpy(buffer, loopback_queue.packets[index], len);
    loopback_queue.head = (loopback_queue.head + 1) % MAX_PACKET_QUEUE;
    loopback_queue.count--;

    return len;
}

static int loopback_start(network_interface_t *iface) {
    (void)iface; // Unused - loopback is always ready
    // Loopback is always ready
    return NET_SUCCESS;
}

static int loopback_stop(network_interface_t *iface) {
    (void)iface; // Unused
    // Clear packet queue
    loopback_queue.head = 0;
    loopback_queue.tail = 0;
    loopback_queue.count = 0;
    return NET_SUCCESS;
}

static int loopback_init_dev(network_interface_t *iface) {
    (void)iface; // Unused
    // Initialize loopback queue
    loopback_queue.head = 0;
    loopback_queue.tail = 0;
    loopback_queue.count = 0;
    return NET_SUCCESS;
}

static void loopback_set_mac(network_interface_t *iface, uint8_t *mac) {
    if (mac) {
        memcpy(iface->mac_address, mac, 6);
    }
}

static void loopback_get_mac(network_interface_t *iface, uint8_t *mac) {
    if (mac) {
        memcpy(mac, iface->mac_address, 6);
    }
}

static netdev_ops_t loopback_ops = {
    .init = loopback_init_dev,
    .start = loopback_start,
    .stop = loopback_stop,
    .send = loopback_send,
    .receive = loopback_receive,
    .set_mac = loopback_set_mac,
    .get_mac = loopback_get_mac
};

int netdev_register(const char *name, netdev_ops_t *ops, uint8_t *mac_addr, uint32_t ip, uint32_t netmask, uint32_t gateway) {
    if (!name || !ops) {
        return NET_INVALID_PARAM;
    }

    // Allocate network interface
    static network_interface_t interfaces[MAX_NETWORK_INTERFACES];
    static int interface_count = 0;
    
    if (interface_count >= MAX_NETWORK_INTERFACES) {
        return NET_ERROR;
    }

    network_interface_t *iface = &interfaces[interface_count];
    interface_count++;

    // Initialize interface
    if (mac_addr) {
        memcpy(iface->mac_address, mac_addr, 6);
    } else {
        // Generate default MAC
        iface->mac_address[0] = 0x02; // Locally administered
        iface->mac_address[1] = 0x00;
        iface->mac_address[2] = 0x00;
        iface->mac_address[3] = 0x00;
        iface->mac_address[4] = 0x00;
        iface->mac_address[5] = interface_count;
    }

    iface->ip_address = ip;
    iface->subnet_mask = netmask;
    iface->gateway = gateway;
    iface->active = false;
    netdev_strncpy(iface->name, name, sizeof(iface->name) - 1);
    iface->name[sizeof(iface->name) - 1] = '\0';

    // Set operations
    iface->send_packet = ops->send;
    iface->receive_packet = ops->receive;

    // Initialize device
    if (ops->init) {
        ops->init(iface);
    }

    // Register with network stack
    network_register_interface(iface);

    // Start device
    if (ops->start) {
        ops->start(iface);
    }

    return NET_SUCCESS;
}

network_interface_t *netdev_get_by_name(const char *name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < MAX_NETWORK_INTERFACES; i++) {
        network_interface_t *iface = network_get_interface(i);
        if (iface && netdev_strcmp(iface->name, name) == 0) {
            return iface;
        }
    }

    return NULL;
}

void netdev_list(void) {
    DEBUG_INFO("=== Network Interfaces ===\n");
    for (int i = 0; i < MAX_NETWORK_INTERFACES; i++) {
        network_interface_t *iface = network_get_interface(i);
        if (iface && iface->name[0] != '\0') {
            DEBUG_INFO("  %s: %s MAC=%02x:%02x:%02x:%02x:%02x:%02x IP=%d.%d.%d.%d\n",
                      iface->name,
                      iface->active ? "UP" : "DOWN",
                      iface->mac_address[0], iface->mac_address[1],
                      iface->mac_address[2], iface->mac_address[3],
                      iface->mac_address[4], iface->mac_address[5],
                      (iface->ip_address >> 24) & 0xFF,
                      (iface->ip_address >> 16) & 0xFF,
                      (iface->ip_address >> 8) & 0xFF,
                      iface->ip_address & 0xFF);
        }
    }
    DEBUG_INFO("=== End Network Interfaces ===\n");
}

int loopback_init(void) {
    uint8_t loopback_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    return netdev_register("lo", &loopback_ops, loopback_mac, 
                          0x7F000001, // 127.0.0.1
                          0xFF000000, // 255.0.0.0
                          0x7F000001  // 127.0.0.1
                          );
}

// Ethernet interface operations - connected to E1000 driver
static int ethernet_send(network_interface_t *iface, void *data, size_t len) {
    // Call E1000 driver to send the packet
    return e1000_send_packet(iface, data, len);
}

static int ethernet_receive(network_interface_t *iface, void *buffer, size_t max_len) {
    // Call E1000 driver to receive a packet
    return e1000_receive_packet(iface, buffer, max_len);
}

static int ethernet_start(network_interface_t *iface) {
    (void)iface; // Avoid unused parameter warning
    
    // Start ethernet interface
    return 0;
}

static int ethernet_stop(network_interface_t *iface) {
    if (iface) {
        iface->active = false;
    }
    // E1000 driver handles its own state - just mark interface inactive
    // In a full implementation, we would call e1000_disable_interrupts here
    return 0;
}

static int ethernet_init_dev(network_interface_t *iface) {
    if (!iface) {
        return NET_INVALID_PARAM;
    }
    // E1000 driver is already initialized by e1000_init() before netdev registration
    // Just mark the interface as ready
    iface->active = true;
    return NET_SUCCESS;
}

static netdev_ops_t ethernet_ops = {
    .send = ethernet_send,
    .receive = ethernet_receive,
    .start = ethernet_start,
    .stop = ethernet_stop,
    .init = ethernet_init_dev
};

// Create a real ethernet interface using E1000 driver
int ethernet_init(void) {
    // Try to initialize E1000 driver (PCI should already be initialized)
    if (e1000_init() == 0) {
        // E1000 device found and initialized, register it as network device
        return e1000_register_netdev();
    }
    
    // Fallback to simulated ethernet interface if no E1000 found
    uint8_t ethernet_mac[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56}; // Locally administered MAC
    return netdev_register("eth0", &ethernet_ops, ethernet_mac,
                          0x00000000, // 0.0.0.0 (will be set by DHCP)
                          0x00000000, // 0.0.0.0 (will be set by DHCP)
                          0x00000000  // 0.0.0.0 (will be set by DHCP)
                          );
}


