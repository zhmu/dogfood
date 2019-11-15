/* ANSI C namespace clean utility typedefs */

/* This file defines various typedefs needed by the system calls that support
   the C library.  Basically, they're just the POSIX versions with an '_'
   prepended.  Targets shall use <machine/_types.h> to define their own
   internal types if desired.

   There are three define patterns used for type definitions.  Lets assume
   xyz_t is a user type.

   The internal type definition uses __machine_xyz_t_defined.  It is defined by
   <machine/_types.h> to disable a default definition in <sys/_types.h>. It
   must not be used in other files.

   User type definitions are guarded by __xyz_t_defined in glibc and
   _XYZ_T_DECLARED in BSD compatible systems.
*/

#ifndef	_SYS__TYPES_H
#define _SYS__TYPES_H

#include <newlib.h>
#include <sys/config.h>
#include <machine/_types.h>
#include <sys/lock.h>

#include <dogfood/types.h>

#ifndef __machine_ssize_t_defined
#ifdef __SIZE_TYPE__
/* If __SIZE_TYPE__ is defined (gcc) we define ssize_t based on size_t.
   We simply change "unsigned" to "signed" for this single definition
   to make sure ssize_t and size_t only differ by their signedness. */
#define unsigned signed
typedef __SIZE_TYPE__ _ssize_t;
#undef unsigned
#else
#if defined(__INT_MAX__) && __INT_MAX__ == 2147483647
typedef int _ssize_t;
#else
typedef long _ssize_t;
#endif
#endif
#endif

typedef __off_t _off_t;
typedef _ssize_t __ssize_t;
typedef _off_t _fpos_t;

#define __need_wint_t
#include <stddef.h>

#ifndef __machine_mbstate_t_defined
/* Conversion state information.  */
typedef struct
{
  int __count;
  union
  {
    wint_t __wch;
    unsigned char __wchb[4];
  } __value;		/* Value so far.  */
} _mbstate_t;
#endif

#ifndef __machine_flock_t_defined
typedef _LOCK_RECURSIVE_T _flock_t;
#endif

#ifndef __machine_iconv_t_defined
/* Iconv descriptor type */
typedef void *_iconv_t;
#endif

#ifndef __machine_clock_t_defined
#define	_CLOCK_T_	unsigned long	/* clock() */
#endif

typedef	_CLOCK_T_	__clock_t;

#if defined(_USE_LONG_TIME_T) || __LONG_MAX__ > 0x7fffffffL
#define	_TIME_T_ long
#else
#define	_TIME_T_ __int_least64_t
#endif
typedef	_TIME_T_	__time_t;

#ifndef __machine_clockid_t_defined
#define	_CLOCKID_T_ 	unsigned long
#endif

typedef	_CLOCKID_T_	__clockid_t;

#define	_TIMER_T_	unsigned long
typedef	_TIMER_T_	__timer_t;

#ifndef __machine_sa_family_t_defined
typedef	__uint8_t	__sa_family_t;
#endif

#ifndef __machine_socklen_t_defined
typedef	__uint32_t	__socklen_t;
#endif

typedef	int		__nl_item;
typedef	unsigned short	__nlink_t;
typedef	long		__suseconds_t;	/* microseconds (signed) */
typedef	unsigned long	__useconds_t;	/* microseconds (unsigned) */

#ifdef __GNUCLIKE_BUILTIN_VARARGS
typedef	__builtin_va_list	__va_list;
#else
typedef	char *			__va_list;
#endif /* __GNUCLIKE_BUILTIN_VARARGS */

#endif	/* _SYS__TYPES_H */
