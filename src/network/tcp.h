#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "network.h"
#include "ip.h"

// TCP constants
#define TCP_HEADER_LEN 20
#define TCP_MAX_PAYLOAD (IP_PACKET_SIZE - IP_HEADER_LEN - TCP_HEADER_LEN)
#define TCP_WINDOW_SIZE 65535
#define TCP_MSS 1460

// TCP flags
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

// TCP states
typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

// TCP header
typedef struct __attribute__((packed)) tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset_reserved;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} tcp_header_t;

// TCP packet structure
typedef struct tcp_packet {
    tcp_header_t header;
    uint8_t payload[TCP_MAX_PAYLOAD];
} tcp_packet_t;

// TCP connection structure
typedef struct tcp_connection {
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    tcp_state_t state;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t window_size;
    bool active;
    
    // Callbacks
    void (*on_connect)(struct tcp_connection *conn);
    void (*on_data)(struct tcp_connection *conn, void *data, size_t len);
    void (*on_close)(struct tcp_connection *conn);
} tcp_connection_t;

// Function prototypes
int tcp_init(void);
int tcp_send_packet(network_interface_t *iface, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, 
                   uint32_t seq, uint32_t ack, uint8_t flags, void *payload, size_t payload_len);
void tcp_process_packet(network_interface_t *iface, uint32_t src_ip, uint32_t dest_ip, tcp_packet_t *packet);
tcp_connection_t *tcp_create_connection(void);
int tcp_connect(tcp_connection_t *conn, uint32_t remote_ip, uint16_t remote_port);
int tcp_listen(uint16_t port);
int tcp_send(tcp_connection_t *conn, void *data, size_t len);
void tcp_close(tcp_connection_t *conn);
uint16_t tcp_checksum(tcp_header_t *header, uint32_t src_ip, uint32_t dest_ip, size_t len);

#endif // TCP_H
