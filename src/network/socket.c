#include "socket.h"
#include "udp.h"
#include "tcp.h"
#include "../memory/memory.h"

#define MAX_SOCKETS 64

static socket_t sockets[MAX_SOCKETS];
static bool socket_used[MAX_SOCKETS];
static int next_socket_fd = 0;

static int allocate_socket_fd(void) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!socket_used[i]) {
            socket_used[i] = true;
            return i;
        }
    }
    return -1; // No free sockets
}

static void free_socket_fd(int sockfd) {
    if (sockfd >= 0 && sockfd < MAX_SOCKETS) {
        socket_used[sockfd] = false;
        memset(&sockets[sockfd], 0, sizeof(socket_t));
    }
}

static socket_t *get_socket(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_used[sockfd]) {
        return NULL;
    }
    return &sockets[sockfd];
}

int socket_create(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        return -1; // Only IPv4 supported
    }

    int sockfd = allocate_socket_fd();
    if (sockfd < 0) {
        return -1;
    }

    socket_t *sock = &sockets[sockfd];
    sock->type = type;
    sock->protocol = protocol;
    sock->bound = false;
    sock->connected = false;
    sock->listening = false;

    switch (type) {
        case SOCK_DGRAM:
            sock->impl.udp_socket = udp_create_socket();
            if (!sock->impl.udp_socket) {
                free_socket_fd(sockfd);
                return -1;
            }
            break;
            
        case SOCK_STREAM:
            sock->impl.tcp_connection = tcp_create_connection();
            if (!sock->impl.tcp_connection) {
                free_socket_fd(sockfd);
                return -1;
            }
            break;
            
        default:
            free_socket_fd(sockfd);
            return -1;
    }

    return sockfd;
}

int socket_bind(int sockfd, const sockaddr_t *addr, int addrlen) {
    socket_t *sock = get_socket(sockfd);
    if (!sock || !addr || addrlen < sizeof(sockaddr_in_t)) {
        return -1;
    }

    const sockaddr_in_t *addr_in = (const sockaddr_in_t *)addr;
    uint16_t port = ntohs(addr_in->sin_port);

    switch (sock->type) {
        case SOCK_DGRAM:
            if (udp_bind((udp_socket_t *)sock->impl.udp_socket, port) == NET_SUCCESS) {
                sock->bound = true;
                return 0;
            }
            break;
            
        case SOCK_STREAM:
            // For TCP, binding is handled differently
            sock->bound = true;
            return 0;
    }

    return -1;
}

int socket_listen(int sockfd, int backlog) {
    (void)backlog; // Unused parameter
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || sock->type != SOCK_STREAM || !sock->bound) {
        return -1;
    }

    // Extract port from bound address (simplified)
    uint16_t port = 80; // Default port, should be extracted from bind
    
    if (tcp_listen(port) == NET_SUCCESS) {
        sock->listening = true;
        return 0;
    }

    return -1;
}

int socket_accept(int sockfd, sockaddr_t *addr, int *addrlen) {
    (void)addr;    // Unused parameter
    (void)addrlen; // Unused parameter
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || sock->type != SOCK_STREAM || !sock->listening) {
        return -1;
    }

    // This is a simplified implementation
    // In a real system, this would block until a connection arrives
    return -1; // Not implemented for this basic example
}

int socket_connect(int sockfd, const sockaddr_t *addr, int addrlen) {
    socket_t *sock = get_socket(sockfd);
    if (!sock || !addr || addrlen < sizeof(sockaddr_in_t)) {
        return -1;
    }

    const sockaddr_in_t *addr_in = (const sockaddr_in_t *)addr;
    uint32_t ip = ntohl(addr_in->sin_addr);
    uint16_t port = ntohs(addr_in->sin_port);

    switch (sock->type) {
        case SOCK_DGRAM:
            if (udp_connect((udp_socket_t *)sock->impl.udp_socket, ip, port) == NET_SUCCESS) {
                sock->connected = true;
                return 0;
            }
            break;
            
        case SOCK_STREAM:
            if (tcp_connect((tcp_connection_t *)sock->impl.tcp_connection, ip, port) == NET_SUCCESS) {
                sock->connected = true;
                return 0;
            }
            break;
    }

    return -1;
}

int socket_send(int sockfd, const void *buf, size_t len, int flags) {
    (void)flags; // Unused parameter
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || !buf || len == 0) {
        return -1;
    }

    switch (sock->type) {
        case SOCK_DGRAM:
            if (sock->connected) {
                return udp_send((udp_socket_t *)sock->impl.udp_socket, (void *)buf, len);
            }
            break;
            
        case SOCK_STREAM:
            if (sock->connected) {
                return tcp_send((tcp_connection_t *)sock->impl.tcp_connection, (void *)buf, len);
            }
            break;
    }

    return -1;
}

int socket_recv(int sockfd, void *buf, size_t len, int flags) {
    (void)flags; // Unused parameter
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || !buf || len == 0) {
        return -1;
    }

    switch (sock->type) {
        case SOCK_DGRAM:
            // UDP sockets use the recvfrom path or callbacks
            // For recv on a connected UDP socket, we'd need a receive buffer
            // This is a limitation - UDP data arrives via callbacks in udp_process_packet
            return -1; // No data currently queued
            
        case SOCK_STREAM:
            // TCP sockets would read from the connection's receive buffer
            // This requires a receive buffer implementation in tcp_connection_t
            if (sock->connected && sock->impl.tcp_connection) {
                // In a full implementation, this would read from TCP receive buffer
                // For now, return 0 to indicate no data ready (non-blocking behavior)
                return 0;
            }
            break;
    }

    return -1;
}

int socket_sendto(int sockfd, const void *buf, size_t len, int flags, const sockaddr_t *dest_addr, int addrlen) {
    (void)flags; // Unused parameter
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || !buf || len == 0 || !dest_addr || addrlen < (int)sizeof(sockaddr_in_t)) {
        return -1;
    }

    if (sock->type != SOCK_DGRAM) {
        return -1; // sendto only for UDP
    }

    const sockaddr_in_t *addr_in = (const sockaddr_in_t *)dest_addr;
    uint32_t ip = ntohl(addr_in->sin_addr);
    uint16_t port = ntohs(addr_in->sin_port);

    return udp_sendto((udp_socket_t *)sock->impl.udp_socket, (void *)buf, len, ip, port);
}

int socket_recvfrom(int sockfd, void *buf, size_t len, int flags, sockaddr_t *src_addr, int *addrlen) {
    (void)flags;    // Unused parameter
    (void)src_addr; // Unused parameter
    (void)addrlen;  // Unused parameter
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || !buf || len == 0) {
        return -1;
    }

    if (sock->type != SOCK_DGRAM) {
        return -1; // recvfrom only for UDP
    }

    // This is a simplified implementation
    // In a real system, this would block until data arrives
    return -1;
}

int socket_close(int sockfd) {
    socket_t *sock = get_socket(sockfd);
    if (!sock) {
        return -1;
    }

    switch (sock->type) {
        case SOCK_DGRAM:
            udp_close((udp_socket_t *)sock->impl.udp_socket);
            break;
            
        case SOCK_STREAM:
            tcp_close((tcp_connection_t *)sock->impl.tcp_connection);
            break;
    }

    free_socket_fd(sockfd);
    return 0;
}

// Utility functions
uint32_t inet_addr(const char *cp) {
    if (!cp) {
        return 0xFFFFFFFF; // INADDR_NONE
    }
    
    uint32_t octets[4] = {0};
    int octet_idx = 0;
    uint32_t current = 0;
    
    for (const char *p = cp; *p != '\0' && octet_idx < 4; p++) {
        if (*p >= '0' && *p <= '9') {
            current = current * 10 + (*p - '0');
            if (current > 255) {
                return 0xFFFFFFFF; // Invalid
            }
        } else if (*p == '.') {
            octets[octet_idx++] = current;
            current = 0;
        } else {
            return 0xFFFFFFFF; // Invalid character
        }
    }
    
    if (octet_idx < 4) {
        octets[octet_idx] = current;
    }
    
    if (octet_idx != 3) {
        return 0xFFFFFFFF; // Invalid format
    }
    
    // Return in network byte order
    return htonl((octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3]);
}

char *inet_ntoa(uint32_t addr) {
    static char buffer[16];
    
    // addr is in network byte order, convert to host order
    uint32_t host_addr = ntohl(addr);
    
    uint8_t octets[4];
    octets[0] = (host_addr >> 24) & 0xFF;
    octets[1] = (host_addr >> 16) & 0xFF;
    octets[2] = (host_addr >> 8) & 0xFF;
    octets[3] = host_addr & 0xFF;
    
    // Format as "xxx.xxx.xxx.xxx"
    char *p = buffer;
    for (int i = 0; i < 4; i++) {
        uint8_t val = octets[i];
        
        if (val >= 100) {
            *p++ = '0' + (val / 100);
            val %= 100;
            *p++ = '0' + (val / 10);
            *p++ = '0' + (val % 10);
        } else if (val >= 10) {
            *p++ = '0' + (val / 10);
            *p++ = '0' + (val % 10);
        } else {
            *p++ = '0' + val;
        }
        
        if (i < 3) {
            *p++ = '.';
        }
    }
    *p = '\0';
    
    return buffer;
}

uint16_t htons(uint16_t hostshort) {
    return __builtin_bswap16(hostshort);
}

uint16_t ntohs(uint16_t netshort) {
    return __builtin_bswap16(netshort);
}

uint32_t htonl(uint32_t hostlong) {
    return __builtin_bswap32(hostlong);
}

uint32_t ntohl(uint32_t netlong) {
    return __builtin_bswap32(netlong);
}
