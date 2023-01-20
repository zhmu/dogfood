#ifndef __SYS_SELECT_H__
#define __SYS_SELECT_H__

#include <sys/types.h>
#include <dogfood/select.h>

#ifdef __cplusplus
extern "C" {
#endif

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);

#ifdef __cplusplus
}
#endif

#endif // __SYS_SELECT_H__
