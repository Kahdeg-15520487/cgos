#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Socket types
#define SOCK_STREAM 1  // TCP
#define SOCK_DGRAM 2   // UDP

// Address families
#define AF_INET 2

// Socket address structure
typedef struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
} sockaddr_in_t;

typedef struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
} sockaddr_t;

// Socket structure
typedef struct socket {
    int type;
    int protocol;
    bool bound;
    bool connected;
    bool listening;
    union {
        void *udp_socket;
        void *tcp_connection;
    } impl;
} socket_t;

// Function prototypes
int socket_create(int domain, int type, int protocol);
int socket_bind(int sockfd, const sockaddr_t *addr, int addrlen);
int socket_listen(int sockfd, int backlog);
int socket_accept(int sockfd, sockaddr_t *addr, int *addrlen);
int socket_connect(int sockfd, const sockaddr_t *addr, int addrlen);
int socket_send(int sockfd, const void *buf, size_t len, int flags);
int socket_recv(int sockfd, void *buf, size_t len, int flags);
int socket_sendto(int sockfd, const void *buf, size_t len, int flags, const sockaddr_t *dest_addr, int addrlen);
int socket_recvfrom(int sockfd, void *buf, size_t len, int flags, sockaddr_t *src_addr, int *addrlen);
int socket_close(int sockfd);

// Utility functions
uint32_t inet_addr(const char *cp);
char *inet_ntoa(uint32_t addr);
uint16_t htons(uint16_t hostshort);
uint16_t ntohs(uint16_t netshort);
uint32_t htonl(uint32_t hostlong);
uint32_t ntohl(uint32_t netlong);

#endif // SOCKET_H
