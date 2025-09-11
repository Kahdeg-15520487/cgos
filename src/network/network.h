#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Network Configuration
#define MAX_NETWORK_INTERFACES 4
#define ETHERNET_FRAME_SIZE 1518
#define IP_PACKET_SIZE 1500
#define MAX_SOCKETS 64
#define MAX_CONNECTIONS 32

// Error codes
#define NET_SUCCESS 0
#define NET_ERROR -1
#define NET_TIMEOUT -2
#define NET_BUFFER_FULL -3
#define NET_INVALID_PARAM -4

// Network interface structure
typedef struct network_interface {
    uint8_t mac_address[6];
    uint32_t ip_address;
    uint32_t subnet_mask;
    uint32_t gateway;
    bool active;
    char name[16];
    void (*send_packet)(struct network_interface *iface, void *data, size_t len);
    int (*receive_packet)(struct network_interface *iface, void *buffer, size_t max_len);
} network_interface_t;

// Function prototypes
int network_init(void);
int network_register_interface(network_interface_t *iface);
network_interface_t *network_get_interface(int index);
int network_send_raw(network_interface_t *iface, void *data, size_t len);
int network_receive_raw(network_interface_t *iface, void *buffer, size_t max_len);
void network_process_packets(void);

// Utility functions
network_interface_t *network_find_interface_by_ip(uint32_t ip);
network_interface_t *network_find_route(uint32_t dest_ip);

#endif // NETWORK_H
