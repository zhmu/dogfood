#include <dogfood/fcntl.h>

#ifndef	_SYS__DEFAULT_FCNTL_H_
#ifdef __cplusplus
extern "C" {
#endif
#define	_SYS__DEFAULT_FCNTL_H_
#include <_ansi.h>
#include <sys/cdefs.h>
#define	_FOPEN		(-1)	/* from sys/file.h, kernel use only */
#define	_FREAD		0x0001	/* read enabled */
#define	_FWRITE		0x0002	/* write enabled */
#define	_FAPPEND	0x0008	/* append (writes guaranteed at the end) */
#define	_FMARK		0x0010	/* internal; mark during gc() */
#define	_FDEFER		0x0020	/* internal; defer for next gc pass */
#define	_FASYNC		0x0040	/* signal pgrp when data ready */
#define	_FSHLOCK	0x0080	/* BSD flock() shared lock present */
#define	_FEXLOCK	0x0100	/* BSD flock() exclusive lock present */
#define	_FCREAT		0x0200	/* open with file create */
#define	_FTRUNC		0x0400	/* open with truncation */
#define	_FEXCL		0x0800	/* error on open if file exists */
#define	_FNBIO		0x1000	/* non blocking I/O (sys5 style) */
#define	_FSYNC		0x2000	/* do all writes synchronously */
#define	_FNONBLOCK	0x4000	/* non blocking I/O (POSIX style) */
#define	_FNDELAY	_FNONBLOCK	/* non blocking I/O (4.2 style) */
#define	_FNOCTTY	0x8000	/* don't assign a ctty on this open */
#if defined (__CYGWIN__)
#define	_FBINARY	0x10000
#define	_FTEXT		0x20000
#endif
#define	_FNOINHERIT	0x40000
#define	_FDIRECT	0x80000
#define	_FNOFOLLOW	0x100000
#define	_FDIRECTORY	0x200000
#define	_FEXECSRCH	0x400000
#if defined (__CYGWIN__)
#define	_FTMPFILE	0x800000
#define	_FNOATIME	0x1000000
#endif

#define	O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)

#if __MISC_VISIBLE

/*
 * Flags that work for fcntl(fd, F_SETFL, FXXXX)
 */
#define	FAPPEND		_FAPPEND
#define	FSYNC		_FSYNC
#define	FASYNC		_FASYNC
#define	FNBIO		_FNBIO
#define	FNONBIO		_FNONBLOCK	/* XXX fix to be NONBLOCK everywhere */
#define	FNDELAY		_FNDELAY

/*
 * Flags that are disallowed for fcntl's (FCNTLCANT);
 * used for opens, internal state, or locking.
 */
#define	FREAD		_FREAD
#define	FWRITE		_FWRITE
#define	FMARK		_FMARK
#define	FDEFER		_FDEFER
#define	FSHLOCK		_FSHLOCK
#define	FEXLOCK		_FEXLOCK

/*
 * The rest of the flags, used only for opens
 */
#define	FOPEN		_FOPEN
#define	FCREAT		_FCREAT
#define	FTRUNC		_FTRUNC
#define	FEXCL		_FEXCL
#define	FNOCTTY		_FNOCTTY

#endif	/* __MISC_VISIBLE */

#if __BSD_VISIBLE
#define	FNONBLOCK	_FNONBLOCK
#endif	/* __BSD_VISIBLE */

/* XXX close on exec request; must match UF_EXCLOSE in user.h */
#define	FD_CLOEXEC	1	/* posix */

#if __BSD_VISIBLE
/* lock operations for flock(2) */
#define	LOCK_SH		0x01		/* shared file lock */
#define	LOCK_EX		0x02		/* exclusive file lock */
#define	LOCK_NB		0x04		/* don't block when locking */
#define	LOCK_UN		0x08		/* unlock file */
#endif

/*#include <sys/stdtypes.h>*/

#ifndef __CYGWIN__
/* file segment locking set data type - information passed to system by user */
struct flock {
	short	l_type;		/* F_RDLCK, F_WRLCK, or F_UNLCK */
	short	l_whence;	/* flag to choose starting offset */
	long	l_start;	/* relative offset, in bytes */
	long	l_len;		/* length, in bytes; 0 means lock to EOF */
	short	l_pid;		/* returned with F_GETLK */
	short	l_xxx;		/* reserved for future use */
};
#endif /* __CYGWIN__ */

#if __MISC_VISIBLE
/* extended file segment locking set data type */
struct eflock {
	short	l_type;		/* F_RDLCK, F_WRLCK, or F_UNLCK */
	short	l_whence;	/* flag to choose starting offset */
	long	l_start;	/* relative offset, in bytes */
	long	l_len;		/* length, in bytes; 0 means lock to EOF */
	short	l_pid;		/* returned with F_GETLK */
	short	l_xxx;		/* reserved for future use */
	long	l_rpid;		/* Remote process id wanting this lock */
	long	l_rsys;		/* Remote system id wanting this lock */
};
#endif	/* __MISC_VISIBLE */

#include <sys/types.h>
#include <sys/stat.h>		/* sigh. for the mode bits for open/creat */

extern int open (const char *, int, ...);
#if __ATFILE_VISIBLE
extern int openat (int, const char *, int, ...);
#endif
extern int creat (const char *, mode_t);
extern int fcntl (int, int, ...);
#if __BSD_VISIBLE
extern int flock (int, int);
#endif
#if __GNU_VISIBLE
#include <sys/time.h>
extern int futimesat (int, const char *, const struct timeval *);
#endif

/* Provide _<systemcall> prototypes for functions provided by some versions
   of newlib.  */
#ifdef _COMPILING_NEWLIB
extern int _open (const char *, int, ...);
extern int _fcntl (int, int, ...);
#ifdef __LARGE64_FILES
extern int _open64 (const char *, int, ...);
#endif
#endif

#ifdef __cplusplus
}
#endif
#endif	/* !_SYS__DEFAULT_FCNTL_H_ */
