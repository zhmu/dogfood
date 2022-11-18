#ifndef __DOGFOOD_SOCKET_H__
#define __DOGFOOD_SOCKET_H__

typedef int socklen_t;
typedef unsigned int sa_family_t;

#define SOCK_DGRAM 0
#define SOCK_RAW 1
#define SOCK_SEQPACKET 2
#define SOCK_STREAM 3

#define PF_UNSPEC 0
#define PF_INET 1
#define PF_LOCAL 2
#define PF_INET6 3

#define AF_UNSPEC PF_UNSPEC
#define AF_INET PF_INET
#define AF_LOCAL PF_LOCAL
#define AF_INET6 PF_INET6

#define AF_UNIX AF_LOCAL

#define SO_BROADCAST 1
#define SO_REUSEADDR 2

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[];
};

#define SOMAXCONN 1

#endif // __DOGFOOD_SOCKET_H__
