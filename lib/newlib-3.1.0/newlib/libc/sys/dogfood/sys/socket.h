#ifndef __SYS_SOCKET_H__
#define __SYS_SOCKET_H__

#include <dogfood/socket.h>
#include <sys/types.h> // for ssize_t

#ifdef __cplusplus
extern "C" {
#endif

int accept(int socket, struct sockaddr* address, socklen_t* address_len);
int bind(int socket, const struct sockaddr* address, socklen_t address_len);
int connect(int socket, const struct sockaddr* address, socklen_t address_len);
int listen(int socket, int backlog);
int socket(int domain, int type, int protocol);

ssize_t recv(int socket, void* buffer, size_t length, int flags);
ssize_t recvfrom(int socket, void* buffer, size_t length, int flags, struct sockaddr* address, socklen_t* address_len);
ssize_t send(int socket, const void* buffer, size_t length, int flags);
int setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len);
int getsockopt(int socket, int level, int option_name, void* option_value, socklen_t* option_len);

#ifdef __cplusplus
}
#endif

#endif // __SYS_SOCKET_H__
