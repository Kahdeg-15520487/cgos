#include "tcp.h"
#include "ip.h"
#include "../memory/memory.h"

static tcp_connection_t connections[MAX_CONNECTIONS];
static int connection_count = 0;
static uint32_t tcp_sequence_number = 1000;

int tcp_init(void) {
    // Initialize connections
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connections[i].local_ip = 0;
        connections[i].local_port = 0;
        connections[i].remote_ip = 0;
        connections[i].remote_port = 0;
        connections[i].state = TCP_CLOSED;
        connections[i].seq_num = 0;
        connections[i].ack_num = 0;
        connections[i].window_size = TCP_WINDOW_SIZE;
        connections[i].active = false;
        connections[i].on_connect = NULL;
        connections[i].on_data = NULL;
        connections[i].on_close = NULL;
    }
    connection_count = 0;
    return NET_SUCCESS;
}

int tcp_send_packet(network_interface_t *iface, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, 
                   uint32_t seq, uint32_t ack, uint8_t flags, void *payload, size_t payload_len) {
    if (!iface) {
        return NET_INVALID_PARAM;
    }

    if (payload_len > TCP_MAX_PAYLOAD) {
        return NET_ERROR;
    }

    tcp_packet_t packet;
    
    // Build TCP header
    packet.header.src_port = __builtin_bswap16(src_port);
    packet.header.dest_port = __builtin_bswap16(dest_port);
    packet.header.seq_num = __builtin_bswap32(seq);
    packet.header.ack_num = __builtin_bswap32(ack);
    packet.header.data_offset_reserved = (TCP_HEADER_LEN / 4) << 4;
    packet.header.flags = flags;
    packet.header.window = __builtin_bswap16(TCP_WINDOW_SIZE);
    packet.header.checksum = 0;
    packet.header.urgent_ptr = 0;

    // Copy payload if any
    if (payload && payload_len > 0) {
        memcpy(packet.payload, payload, payload_len);
    }

    // Calculate checksum
    packet.header.checksum = tcp_checksum(&packet.header, iface->ip_address, dest_ip, TCP_HEADER_LEN + payload_len);

    // Send via IP layer
    return ip_send_packet(iface, dest_ip, IP_PROTOCOL_TCP, &packet, TCP_HEADER_LEN + payload_len);
}

void tcp_process_packet(network_interface_t *iface, uint32_t src_ip, uint32_t dest_ip, tcp_packet_t *packet) {
    if (!iface || !packet) {
        return;
    }

    // Convert from network byte order
    uint16_t src_port = __builtin_bswap16(packet->header.src_port);
    uint16_t dest_port = __builtin_bswap16(packet->header.dest_port);
    uint32_t seq_num = __builtin_bswap32(packet->header.seq_num);
    uint32_t ack_num = __builtin_bswap32(packet->header.ack_num);
    uint8_t flags = packet->header.flags;
    uint16_t window = __builtin_bswap16(packet->header.window);

    // Find connection
    tcp_connection_t *conn = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].active &&
            connections[i].local_ip == dest_ip &&
            connections[i].local_port == dest_port &&
            connections[i].remote_ip == src_ip &&
            connections[i].remote_port == src_port) {
            conn = &connections[i];
            break;
        }
    }

    // Handle SYN to listening port
    if (!conn && (flags & TCP_FLAG_SYN) && !(flags & TCP_FLAG_ACK)) {
        // Look for listening connection
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (connections[i].active &&
                connections[i].local_ip == dest_ip &&
                connections[i].local_port == dest_port &&
                connections[i].state == TCP_LISTEN) {
                conn = &connections[i];
                conn->remote_ip = src_ip;
                conn->remote_port = src_port;
                break;
            }
        }
    }

    if (!conn) {
        // No connection found, send RST
        tcp_send_packet(iface, src_ip, dest_port, src_port, 0, seq_num + 1, TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0);
        return;
    }

    // Process packet based on connection state
    switch (conn->state) {
        case TCP_LISTEN:
            if (flags & TCP_FLAG_SYN) {
                // Send SYN-ACK
                conn->ack_num = seq_num + 1;
                conn->seq_num = tcp_sequence_number++;
                tcp_send_packet(iface, src_ip, dest_port, src_port, conn->seq_num, conn->ack_num, 
                              TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                conn->state = TCP_SYN_RECEIVED;
            }
            break;

        case TCP_SYN_SENT:
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                // Received SYN-ACK
                conn->ack_num = seq_num + 1;
                tcp_send_packet(iface, src_ip, dest_port, src_port, conn->seq_num + 1, conn->ack_num, 
                              TCP_FLAG_ACK, NULL, 0);
                conn->state = TCP_ESTABLISHED;
                if (conn->on_connect) {
                    conn->on_connect(conn);
                }
            }
            break;

        case TCP_SYN_RECEIVED:
            if (flags & TCP_FLAG_ACK) {
                conn->state = TCP_ESTABLISHED;
                if (conn->on_connect) {
                    conn->on_connect(conn);
                }
            }
            break;

        case TCP_ESTABLISHED:
            // Handle data and control packets
            if (flags & TCP_FLAG_PSH) {
                // Data packet
                size_t data_len = TCP_HEADER_LEN; // Calculate actual data length
                if (data_len > 0 && conn->on_data) {
                    conn->on_data(conn, packet->payload, data_len);
                }
                // Send ACK
                conn->ack_num = seq_num + data_len;
                tcp_send_packet(iface, src_ip, dest_port, src_port, conn->seq_num, conn->ack_num, 
                              TCP_FLAG_ACK, NULL, 0);
            }
            
            if (flags & TCP_FLAG_FIN) {
                // Connection close initiated by remote
                conn->ack_num = seq_num + 1;
                tcp_send_packet(iface, src_ip, dest_port, src_port, conn->seq_num, conn->ack_num, 
                              TCP_FLAG_ACK, NULL, 0);
                conn->state = TCP_CLOSE_WAIT;
                if (conn->on_close) {
                    conn->on_close(conn);
                }
            }
            break;

        case TCP_CLOSE_WAIT:
            // Application should close connection
            break;

        default:
            break;
    }
}

tcp_connection_t *tcp_create_connection(void) {
    if (connection_count >= MAX_CONNECTIONS) {
        return NULL;
    }

    tcp_connection_t *conn = &connections[connection_count];
    connection_count++;
    
    conn->local_ip = 0;
    conn->local_port = 0;
    conn->remote_ip = 0;
    conn->remote_port = 0;
    conn->state = TCP_CLOSED;
    conn->seq_num = tcp_sequence_number++;
    conn->ack_num = 0;
    conn->window_size = TCP_WINDOW_SIZE;
    conn->active = true;
    conn->on_connect = NULL;
    conn->on_data = NULL;
    conn->on_close = NULL;

    return conn;
}

int tcp_connect(tcp_connection_t *conn, uint32_t remote_ip, uint16_t remote_port) {
    if (!conn || remote_ip == 0 || remote_port == 0) {
        return NET_INVALID_PARAM;
    }

    // Find interface to use
    network_interface_t *iface = network_find_route(remote_ip);
    if (!iface) {
        return NET_ERROR;
    }

    conn->local_ip = iface->ip_address;
    conn->local_port = 32768 + (tcp_sequence_number % 32768); // Ephemeral port
    conn->remote_ip = remote_ip;
    conn->remote_port = remote_port;

    // Send SYN
    conn->state = TCP_SYN_SENT;
    return tcp_send_packet(iface, remote_ip, conn->local_port, remote_port, conn->seq_num, 0, TCP_FLAG_SYN, NULL, 0);
}

int tcp_listen(uint16_t port) {
    tcp_connection_t *conn = tcp_create_connection();
    if (!conn) {
        return NET_ERROR;
    }

    conn->local_port = port;
    conn->state = TCP_LISTEN;

    return NET_SUCCESS;
}

int tcp_send(tcp_connection_t *conn, void *data, size_t len) {
    if (!conn || !data || len == 0 || conn->state != TCP_ESTABLISHED) {
        return NET_INVALID_PARAM;
    }

    // Find interface
    network_interface_t *iface = network_find_route(conn->remote_ip);
    if (!iface) {
        return NET_ERROR;
    }

    // Send data
    int result = tcp_send_packet(iface, conn->remote_ip, conn->local_port, conn->remote_port, 
                                conn->seq_num, conn->ack_num, TCP_FLAG_PSH | TCP_FLAG_ACK, data, len);
    
    if (result == NET_SUCCESS) {
        conn->seq_num += len;
    }

    return result;
}

void tcp_close(tcp_connection_t *conn) {
    if (!conn || conn->state == TCP_CLOSED) {
        return;
    }

    // Find interface
    network_interface_t *iface = network_find_route(conn->remote_ip);
    if (iface && conn->state == TCP_ESTABLISHED) {
        // Send FIN
        tcp_send_packet(iface, conn->remote_ip, conn->local_port, conn->remote_port, 
                       conn->seq_num, conn->ack_num, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        conn->state = TCP_FIN_WAIT_1;
    }

    conn->active = false;
    conn->state = TCP_CLOSED;
}

uint16_t tcp_checksum(tcp_header_t *header, uint32_t src_ip, uint32_t dest_ip, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr;

    // Pseudo header
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dest_ip >> 16) & 0xFFFF;
    sum += dest_ip & 0xFFFF;
    sum += __builtin_bswap16(IP_PROTOCOL_TCP);
    sum += __builtin_bswap16(len);

    // TCP header and data
    ptr = (uint16_t *)header;
    for (size_t i = 0; i < len / 2; i++) {
        sum += ptr[i];
    }

    // Handle odd byte
    if (len & 1) {
        sum += ((uint8_t *)header)[len - 1] << 8;
    }

    // Add carry
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}
