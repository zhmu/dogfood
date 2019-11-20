#ifndef _SYS_ERRNO_H_
#define _SYS_ERRNO_H_

#include <dogfood/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __error_t_defined
typedef int error_t;
#define __error_t_defined 1
#endif

#include <sys/reent.h>

#ifndef _REENT_ONLY
#define errno (*__errno())
extern int* __errno(void);
#endif

/* Please don't use these variables directly.
   Use strerror instead. */
extern __IMPORT const char* const _sys_errlist[];
extern __IMPORT int _sys_nerr;

#define __errno_r(ptr) ((ptr)->_errno)

#ifdef __cplusplus
}
#endif

#endif // _SYS_ERRNO_H_
