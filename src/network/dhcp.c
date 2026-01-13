#include "dhcp.h"
#include "socket.h"
#include "udp.h"
#include "ip.h"
#include "ethernet.h"
#include "../debug/debug.h"
#include <stddef.h>

// Magic cookie for DHCP packets
#define DHCP_MAGIC_COOKIE 0x63825363

// DHCP ports
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

// Global DHCP clients (one per interface)
static dhcp_client_t dhcp_clients[MAX_NETWORK_INTERFACES];
static int dhcp_client_count = 0;

// Simple random number for transaction ID
static uint32_t dhcp_xid_counter = 1;

// Simple time counter (should be integrated with actual timer system)
static uint32_t dhcp_time = 0;

uint32_t dhcp_generate_xid(void) {
    return dhcp_xid_counter++;
}

dhcp_client_t *dhcp_get_client(network_interface_t *iface) {
    for (int i = 0; i < dhcp_client_count; i++) {
        if (dhcp_clients[i].iface == iface) {
            return &dhcp_clients[i];
        }
    }
    return NULL;
}

int dhcp_client_init(network_interface_t *iface) {
    if (!iface || dhcp_client_count >= MAX_NETWORK_INTERFACES) {
        return -1;
    }
    
    dhcp_client_t *client = &dhcp_clients[dhcp_client_count++];
    client->iface = iface;
    client->state = DHCP_STATE_INIT;
    client->transaction_id = dhcp_generate_xid();
    client->server_ip = 0;
    client->offered_ip = 0;
    client->subnet_mask = 0;
    client->gateway = 0;
    client->dns_server = 0;
    client->lease_time = 0;
    client->renewal_time = 0;
    client->rebinding_time = 0;
    client->lease_start_time = 0;
    client->active = false;
    
    return 0;
}

int dhcp_add_option(uint8_t *options, int *offset, uint8_t type, uint8_t len, const void *data) {
    if (*offset + 2 + len >= 312) { // Options field is 312 bytes max
        return -1;
    }
    
    options[(*offset)++] = type;
    options[(*offset)++] = len;
    
    if (data && len > 0) {
        for (int i = 0; i < len; i++) {
            options[(*offset)++] = ((uint8_t*)data)[i];
        }
    }
    
    return 0;
}

int dhcp_client_discover(dhcp_client_t *client) {
    if (!client || !client->iface) {
        return -1;
    }
    
    dhcp_packet_t packet = {0};
    
    // Fill DHCP header
    packet.op = 1;      // BOOTREQUEST
    packet.htype = 1;   // Ethernet
    packet.hlen = 6;    // MAC address length
    packet.hops = 0;
    packet.xid = htonl(client->transaction_id);
    packet.secs = 0;
    packet.flags = htons(0x8000); // Broadcast flag
    packet.ciaddr = 0;  // Client IP (we don't have one yet)
    packet.yiaddr = 0;  // Your IP (server will fill this)
    packet.siaddr = 0;  // Server IP
    packet.giaddr = 0;  // Gateway IP
    packet.magic = htonl(DHCP_MAGIC_COOKIE);
    
    // Copy MAC address to chaddr
    for (int i = 0; i < 6; i++) {
        packet.chaddr[i] = client->iface->mac_address[i];
    }
    
    // Add DHCP options
    int opt_offset = 0;
    uint8_t msg_type = DHCP_DISCOVER;
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_MSG_TYPE, 1, &msg_type);
    
    // Add parameter request list
    uint8_t param_list[] = {
        DHCP_OPTION_SUBNET_MASK,
        DHCP_OPTION_ROUTER,
        DHCP_OPTION_DNS_SERVER,
        DHCP_OPTION_DOMAIN_NAME,
        DHCP_OPTION_BROADCAST_ADDR
    };
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_PARAM_REQUEST, 
                   sizeof(param_list), param_list);
    
    // Add client identifier (MAC address)
    uint8_t client_id[7];
    client_id[0] = 1; // Hardware type (Ethernet)
    for (int i = 0; i < 6; i++) {
        client_id[i + 1] = client->iface->mac_address[i];
    }
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_CLIENT_ID, 7, client_id);
    
    // End option
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_END, 0, NULL);
    
    // Calculate total packet size
    size_t packet_size = sizeof(dhcp_packet_t) - sizeof(packet.options) + opt_offset + 1;
    
    // Send UDP packet to broadcast address (255.255.255.255:67)
    // DHCP client sends from port 68 to server port 67
    int result = udp_send_packet(client->iface, 
                                  0xFFFFFFFF,  // Broadcast: 255.255.255.255
                                  68,          // Source port: DHCP client
                                  67,          // Dest port: DHCP server
                                  &packet, 
                                  packet_size);
    
    if (result < 0) {
        DEBUG_ERROR("DHCP: Failed to send DISCOVER packet\n");
        return -1;
    }
    
    DEBUG_INFO("DHCP: Sent DISCOVER packet (%zu bytes)\n", packet_size);
    client->state = DHCP_STATE_SELECTING;
    return 0;
}

int dhcp_client_request(dhcp_client_t *client) {
    if (!client || !client->iface || client->offered_ip == 0) {
        DEBUG_WARN("DHCP REQUEST: Invalid parameters\\n");
        return -1;
    }
    
    dhcp_packet_t packet = {0};
    
    // Fill DHCP header
    packet.op = 1;      // BOOTREQUEST
    packet.htype = 1;   // Ethernet
    packet.hlen = 6;    // MAC address length
    packet.hops = 0;
    packet.xid = htonl(client->transaction_id);
    packet.secs = 0;
    packet.flags = htons(0x8000); // Broadcast flag
    packet.ciaddr = 0;  // Client IP
    packet.yiaddr = 0;  // Your IP
    packet.siaddr = 0;  // Server IP
    packet.giaddr = 0;  // Gateway IP
    packet.magic = htonl(DHCP_MAGIC_COOKIE);
    
    // Copy MAC address to chaddr
    for (int i = 0; i < 6; i++) {
        packet.chaddr[i] = client->iface->mac_address[i];
    }
    
    // Add DHCP options
    int opt_offset = 0;
    uint8_t msg_type = DHCP_REQUEST;
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_MSG_TYPE, 1, &msg_type);
    
    // Add requested IP address
    uint32_t requested_ip = htonl(client->offered_ip);
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_REQUESTED_IP, 4, &requested_ip);
    
    // Add server identifier
    uint32_t server_id = htonl(client->server_ip);
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_SERVER_ID, 4, &server_id);
    
    // Add client identifier
    uint8_t client_id[7];
    client_id[0] = 1; // Hardware type (Ethernet)
    for (int i = 0; i < 6; i++) {
        client_id[i + 1] = client->iface->mac_address[i];
    }
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_CLIENT_ID, 7, client_id);
    
    // End option
    dhcp_add_option(packet.options, &opt_offset, DHCP_OPTION_END, 0, NULL);
    
    // Calculate total packet size
    size_t packet_size = sizeof(dhcp_packet_t) - sizeof(packet.options) + opt_offset + 1;
    
    // Send UDP packet to broadcast address (255.255.255.255:67)
    int result = udp_send_packet(client->iface, 
                                  0xFFFFFFFF,  // Broadcast: 255.255.255.255
                                  68,          // Source port: DHCP client
                                  67,          // Dest port: DHCP server
                                  &packet, 
                                  packet_size);
    
    if (result < 0) {
        DEBUG_ERROR("DHCP: Failed to send REQUEST packet\\n");
        return -1;
    }
    
    DEBUG_INFO("DHCP: Sent REQUEST packet (%zu bytes)\\n", packet_size);
    client->state = DHCP_STATE_REQUESTING;
    return 0;
}

int dhcp_parse_options(uint8_t *options, size_t len, dhcp_client_t *client, uint8_t *msg_type) {
    size_t i = 0;
    *msg_type = 0;
    
    while (i < len) {
        uint8_t option_type = options[i++];
        
        if (option_type == DHCP_OPTION_PAD) {
            continue;
        }
        
        if (option_type == DHCP_OPTION_END) {
            break;
        }
        
        if (i >= len) break;
        uint8_t option_len = options[i++];
        
        if (i + option_len > len) break;
        
        switch (option_type) {
            case DHCP_OPTION_MSG_TYPE:
                if (option_len == 1) {
                    *msg_type = options[i];
                }
                break;
                
            case DHCP_OPTION_SUBNET_MASK:
                if (option_len == 4) {
                    client->subnet_mask = (options[i] << 24) | (options[i+1] << 16) | 
                                         (options[i+2] << 8) | options[i+3];
                }
                break;
                
            case DHCP_OPTION_ROUTER:
                if (option_len >= 4) {
                    client->gateway = (options[i] << 24) | (options[i+1] << 16) | 
                                     (options[i+2] << 8) | options[i+3];
                }
                break;
                
            case DHCP_OPTION_DNS_SERVER:
                if (option_len >= 4) {
                    client->dns_server = (options[i] << 24) | (options[i+1] << 16) | 
                                        (options[i+2] << 8) | options[i+3];
                }
                break;
                
            case DHCP_OPTION_LEASE_TIME:
                if (option_len == 4) {
                    client->lease_time = (options[i] << 24) | (options[i+1] << 16) | 
                                        (options[i+2] << 8) | options[i+3];
                }
                break;
                
            case DHCP_OPTION_SERVER_ID:
                if (option_len == 4) {
                    client->server_ip = (options[i] << 24) | (options[i+1] << 16) | 
                                       (options[i+2] << 8) | options[i+3];
                }
                break;
                
            case DHCP_OPTION_RENEWAL_TIME:
                if (option_len == 4) {
                    client->renewal_time = (options[i] << 24) | (options[i+1] << 16) | 
                                          (options[i+2] << 8) | options[i+3];
                }
                break;
                
            case DHCP_OPTION_REBINDING_TIME:
                if (option_len == 4) {
                    client->rebinding_time = (options[i] << 24) | (options[i+1] << 16) | 
                                            (options[i+2] << 8) | options[i+3];
                }
                break;
        }
        
        i += option_len;
    }
    
    return 0;
}

void dhcp_client_process_packet(dhcp_client_t *client, dhcp_packet_t *packet, size_t packet_len) {
    DEBUG_INFO("DHCP: Processing received packet (len=%zu)\\n", packet_len);
    
    if (!client || !packet) {
        DEBUG_WARN("DHCP: Invalid client or packet pointer\\n");
        return;
    }
    
    // Minimum DHCP packet size check (header without full options)
    if (packet_len < 240) {  // DHCP header is 236 bytes + 4 bytes magic cookie minimum
        DEBUG_WARN("DHCP: Packet too small (%zu bytes)\\n", packet_len);
        return;
    }
    
    // Check magic cookie
    uint32_t magic = ntohl(packet->magic);
    DEBUG_DEBUG("DHCP: Magic cookie = 0x%08x (expected 0x63825363)\\n", magic);
    if (magic != 0x63825363) {
        DEBUG_WARN("DHCP: Invalid magic cookie\\n");
        return;
    }
    
    // Check if this packet is for us (XID match)
    uint32_t pkt_xid = ntohl(packet->xid);
    DEBUG_DEBUG("DHCP: Packet XID=0x%08x, our XID=0x%08x\\n", pkt_xid, client->transaction_id);
    if (pkt_xid != client->transaction_id) {
        DEBUG_WARN("DHCP: XID mismatch\\n");
        return;
    }
    
    // Parse options to get message type
    uint8_t msg_type = 0;
    dhcp_parse_options(packet->options, 312, client, &msg_type);
    
    DEBUG_INFO("DHCP: Received message type=%d, client state=%d\\n", msg_type, client->state);
    
    // Get offered IP address
    uint32_t offered_ip = ntohl(packet->yiaddr);
    DEBUG_INFO("DHCP: Offered IP = %d.%d.%d.%d\\n",
               (offered_ip >> 24) & 0xFF, (offered_ip >> 16) & 0xFF,
               (offered_ip >> 8) & 0xFF, offered_ip & 0xFF);
    
    switch (client->state) {
        case DHCP_STATE_SELECTING:
            if (msg_type == DHCP_OFFER) {
                DEBUG_INFO("DHCP: Received OFFER! IP=%d.%d.%d.%d\\n",
                          (offered_ip >> 24) & 0xFF, (offered_ip >> 16) & 0xFF,
                          (offered_ip >> 8) & 0xFF, offered_ip & 0xFF);
                // Save offered IP and server info
                client->offered_ip = offered_ip;
                
                // Send DHCP REQUEST
                DEBUG_INFO("DHCP: Sending REQUEST...\\n");
                dhcp_client_request(client);
            } else {
                DEBUG_WARN("DHCP: Expected OFFER (type=2), got type=%d\\n", msg_type);
            }
            break;
            
        case DHCP_STATE_REQUESTING:
            if (msg_type == DHCP_ACK) {
                DEBUG_INFO("DHCP: Received ACK! Configuration complete.\\n");
                // Configuration accepted
                client->iface->ip_address = client->offered_ip;
                client->iface->subnet_mask = client->subnet_mask;
                client->iface->gateway = client->gateway;
                client->state = DHCP_STATE_BOUND;
                client->lease_start_time = dhcp_time;
                client->active = true;
                
                DEBUG_INFO("DHCP: Assigned IP=%d.%d.%d.%d\\n",
                          (client->offered_ip >> 24) & 0xFF, (client->offered_ip >> 16) & 0xFF,
                          (client->offered_ip >> 8) & 0xFF, client->offered_ip & 0xFF);
                
                // If renewal time not specified, use 50% of lease time
                if (client->renewal_time == 0) {
                    client->renewal_time = client->lease_time / 2;
                }
                
                // If rebinding time not specified, use 87.5% of lease time
                if (client->rebinding_time == 0) {
                    client->rebinding_time = (client->lease_time * 7) / 8;
                }
            } else if (msg_type == DHCP_NAK) {
                DEBUG_WARN("DHCP: Received NAK - configuration rejected\\n");
                // Configuration rejected, start over
                client->state = DHCP_STATE_INIT;
                client->offered_ip = 0;
                client->server_ip = 0;
            }
            break;
            
        default:
            DEBUG_WARN("DHCP: Unexpected state %d for incoming packet\\n", client->state);
            break;
    }
}

int dhcp_client_start(dhcp_client_t *client) {
    if (!client) {
        return -1;
    }
    
    client->state = DHCP_STATE_INIT;
    client->transaction_id = dhcp_generate_xid();
    
    return dhcp_client_discover(client);
}

int dhcp_client_release(dhcp_client_t *client) {
    if (!client || !client->active) {
        return -1;
    }
    
    // Send DHCP RELEASE message (implementation would go here)
    
    // Reset client state
    client->iface->ip_address = 0;
    client->iface->subnet_mask = 0;
    client->iface->gateway = 0;
    client->state = DHCP_STATE_INIT;
    client->active = false;
    
    return 0;
}

void dhcp_client_update(dhcp_client_t *client) {
    if (!client || !client->active) {
        return;
    }
    
    uint32_t elapsed = dhcp_time - client->lease_start_time;
    
    switch (client->state) {
        case DHCP_STATE_BOUND:
            if (elapsed >= client->renewal_time) {
                client->state = DHCP_STATE_RENEWING;
                // Send unicast DHCP REQUEST to renew lease
                dhcp_client_request(client);
            }
            break;
            
        case DHCP_STATE_RENEWING:
            if (elapsed >= client->rebinding_time) {
                client->state = DHCP_STATE_REBINDING;
                // Send broadcast DHCP REQUEST
                dhcp_client_request(client);
            }
            break;
            
        case DHCP_STATE_REBINDING:
            if (elapsed >= client->lease_time) {
                // Lease expired, start over
                client->state = DHCP_STATE_INIT;
                client->active = false;
                client->iface->ip_address = 0;
                dhcp_client_start(client);
            }
            break;
            
        default:
            break;
    }
}

// Helper function to simulate DHCP server response for testing
int dhcp_simulate_offer(dhcp_client_t *client, uint32_t offered_ip, uint32_t server_ip) {
    if (!client) {
        return -1;
    }
    
    // Simulate receiving a DHCP OFFER
    client->offered_ip = offered_ip;
    client->server_ip = server_ip;
    client->subnet_mask = 0xFFFFFF00; // 255.255.255.0
    client->gateway = (offered_ip & 0xFFFFFF00) | 0x01; // .1 as gateway
    client->dns_server = 0x08080808; // 8.8.8.8
    client->lease_time = 3600; // 1 hour
    
    return dhcp_client_request(client);
}

// Helper function to simulate DHCP ACK for testing
int dhcp_simulate_ack(dhcp_client_t *client) {
    if (!client || client->offered_ip == 0) {
        return -1;
    }
    
    // Apply the configuration
    client->iface->ip_address = client->offered_ip;
    client->iface->subnet_mask = client->subnet_mask;
    client->iface->gateway = client->gateway;
    client->state = DHCP_STATE_BOUND;
    client->lease_start_time = dhcp_time;
    client->active = true;
    
    return 0;
}
