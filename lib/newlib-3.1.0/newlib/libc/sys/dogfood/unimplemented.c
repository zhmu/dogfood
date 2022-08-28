#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

int getaddrinfo(const char* nodename, const char* servname, const struct addrinfo* hints, struct addrinfo** res)
{
    errno = ENOSYS;
    return EAI_SYSTEM;
}

void freeaddrinfo(struct addrinfo* ai)
{
}

const char* gai_strerror(int ecode)
{
    return "not implemented";
}
