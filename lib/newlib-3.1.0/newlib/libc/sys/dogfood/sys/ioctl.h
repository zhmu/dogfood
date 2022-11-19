#ifndef __DOGFOOD_IOCTL_H__
#define __DOGFOOD_IOCTL_H__

#include <dogfood/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __DOGFOOD_IOCTL_H__
