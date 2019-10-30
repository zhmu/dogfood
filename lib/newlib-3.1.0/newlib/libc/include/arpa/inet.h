#ifndef _ARPA_INET_H_
#define _ARPA_INET_H_

#include <inttypes.h>

uint32_t htonl(uint32_t hostlong);
uint32_t htons(uint32_t hostlong);
uint32_t ntohs(uint32_t hostlong);
uint32_t ntohl(uint32_t hostlong);

#endif //_ARPA_INET_H_
