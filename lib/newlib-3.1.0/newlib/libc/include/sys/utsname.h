#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/types.h>
#include <dogfood/utsname.h>

#ifdef __cplusplus
extern "C" {
#endif

int uname(struct utsname*);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_UTSNAME_H */
