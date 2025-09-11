#ifndef NETDEV_H
#define NETDEV_H

#include "network.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Network device operations
typedef struct netdev_ops {
    int (*init)(network_interface_t *iface);
    int (*start)(network_interface_t *iface);
    int (*stop)(network_interface_t *iface);
    int (*send)(network_interface_t *iface, void *data, size_t len);
    int (*receive)(network_interface_t *iface, void *buffer, size_t max_len);
    void (*set_mac)(network_interface_t *iface, uint8_t *mac);
    void (*get_mac)(network_interface_t *iface, uint8_t *mac);
} netdev_ops_t;

// Function prototypes
int netdev_register(const char *name, netdev_ops_t *ops, uint8_t *mac_addr, uint32_t ip, uint32_t netmask, uint32_t gateway);
network_interface_t *netdev_get_by_name(const char *name);
void netdev_list(void);

// Loopback driver
int loopback_init(void);

#endif // NETDEV_H
