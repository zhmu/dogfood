/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __NETINET_IN_H__
#define __NETINET_IN_H__

#include <sys/socket.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
};

struct in6_addr {
    uint8_t s6_addr[16];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

#define IPPROTO_IP 1
#define IPPROTO_ICMP 2
#define IPPROTO_RAW 3
#define IPPROTO_TCP 4
#define IPPROTO_UDP 5

#define INADDR_ANY 0
#define INADDR_BROADCAST 0xffffffff

#define INET_ADDRSTRLEN 16

#if defined(___i386__) || defined(__x86_64__)
#define ntohl(x) __builtin_bswap32(x)
#define ntohs(x) __builtin_bswap16(x)
#define htonl(x) __builtin_bswap32(x)
#define htons(x) __builtin_bswap32(x)
#endif

#endif /* __NETINET_IN_H__ */
