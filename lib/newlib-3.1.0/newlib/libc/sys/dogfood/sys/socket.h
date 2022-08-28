#ifndef __SYS_SOCKET_H__
#define __SYS_SOCKET_H__

#include <sys/types.h> // for ssize_t

typedef int socklen_t;

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

typedef unsigned int sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[];
};

#define SOMAXCONN 1

#ifdef __cplusplus
extern "C" {
#endif

int accept(int socket, struct sockaddr* address, socklen_t* address_len);
int bind(int socket, const struct sockaddr* address, socklen_t address_len);
int connect(int socket, const struct sockaddr* address, socklen_t address_len);
int listen(int socket, int backlog);
int socket(int domain, int type, int protocol);

ssize_t send(int socket, const void* buffer, size_t length, int flags);

#ifdef __cplusplus
}
#endif

#endif // __SYS_SOCKET_H__
