#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "netdev.h"

// DHCP message types
#define DHCP_DISCOVER   1
#define DHCP_OFFER      2
#define DHCP_REQUEST    3
#define DHCP_DECLINE    4
#define DHCP_ACK        5
#define DHCP_NAK        6
#define DHCP_RELEASE    7
#define DHCP_INFORM     8

// DHCP options
#define DHCP_OPTION_PAD                 0
#define DHCP_OPTION_SUBNET_MASK         1
#define DHCP_OPTION_ROUTER              3
#define DHCP_OPTION_DNS_SERVER          6
#define DHCP_OPTION_DOMAIN_NAME         15
#define DHCP_OPTION_BROADCAST_ADDR      28
#define DHCP_OPTION_REQUESTED_IP        50
#define DHCP_OPTION_LEASE_TIME          51
#define DHCP_OPTION_MSG_TYPE            53
#define DHCP_OPTION_SERVER_ID           54
#define DHCP_OPTION_PARAM_REQUEST       55
#define DHCP_OPTION_RENEWAL_TIME        58
#define DHCP_OPTION_REBINDING_TIME      59
#define DHCP_OPTION_CLIENT_ID           61
#define DHCP_OPTION_END                 255

// DHCP packet structure
typedef struct __attribute__((packed)) dhcp_packet {
    uint8_t op;             // Message op code / message type
    uint8_t htype;          // Hardware address type
    uint8_t hlen;           // Hardware address length
    uint8_t hops;           // Client sets to zero, optionally used by relay agents
    uint32_t xid;           // Transaction ID
    uint16_t secs;          // Seconds elapsed since client began address acquisition
    uint16_t flags;         // Flags
    uint32_t ciaddr;        // Client IP address
    uint32_t yiaddr;        // Your (client) IP address
    uint32_t siaddr;        // IP address of next server to use in bootstrap
    uint32_t giaddr;        // Relay agent IP address
    uint8_t chaddr[16];     // Client hardware address
    uint8_t sname[64];      // Optional server host name
    uint8_t file[128];      // Boot file name
    uint32_t magic;         // Magic cookie (0x63825363)
    uint8_t options[312];   // Optional parameters field
} dhcp_packet_t;

// DHCP client state
typedef enum {
    DHCP_STATE_INIT,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_RENEWING,
    DHCP_STATE_REBINDING,
    DHCP_STATE_INIT_REBOOT
} dhcp_state_t;

// DHCP client context
typedef struct dhcp_client {
    network_interface_t *iface;
    dhcp_state_t state;
    uint32_t transaction_id;
    uint32_t server_ip;
    uint32_t offered_ip;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint32_t dns_server;
    uint32_t lease_time;
    uint32_t renewal_time;
    uint32_t rebinding_time;
    uint32_t lease_start_time;
    bool active;
} dhcp_client_t;

// Function prototypes
int dhcp_client_init(network_interface_t *iface);
int dhcp_client_start(dhcp_client_t *client);
int dhcp_client_discover(dhcp_client_t *client);
int dhcp_client_request(dhcp_client_t *client);
int dhcp_client_release(dhcp_client_t *client);
void dhcp_client_process_packet(dhcp_client_t *client, dhcp_packet_t *packet, size_t packet_len);
void dhcp_client_update(dhcp_client_t *client);
dhcp_client_t *dhcp_get_client(network_interface_t *iface);

// Utility functions
uint32_t dhcp_generate_xid(void);
int dhcp_add_option(uint8_t *options, int *offset, uint8_t type, uint8_t len, const void *data);
int dhcp_parse_options(uint8_t *options, size_t len, dhcp_client_t *client, uint8_t *msg_type);

// Helper functions for testing/simulation
int dhcp_simulate_offer(dhcp_client_t *client, uint32_t offered_ip, uint32_t server_ip);
int dhcp_simulate_ack(dhcp_client_t *client);

#endif // DHCP_H
