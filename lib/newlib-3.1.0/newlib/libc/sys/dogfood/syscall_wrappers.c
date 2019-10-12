#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    /* Creates a new mapping */
    OP_MAP,

    /* Removes a mapping - only va_addr/va_len are used */
    OP_UNMAP,

    /* Change permissions of a mapping - only va_addr/va_len/va_flags are used */
    OP_CHANGE_ACCESS,

    OP_SBRK
} VMOP_OPERATION;

#define VMOP_FLAG_READ 0x0001
#define VMOP_FLAG_WRITE 0x0002
#define VMOP_FLAG_EXECUTE 0x0004

#define VMOP_FLAG_SHARED 0x0008
#define VMOP_FLAG_PRIVATE 0x0010
#define VMOP_FLAG_FD 0x0020
#define VMOP_FLAG_FIXED 0x0040

struct VMOP_OPTIONS {
    size_t vo_size; /* must be sizeof(VMOP_OPTIONS) */
    VMOP_OPERATION vo_op;

    /* Address and length of the mapping */
    void* vo_addr; /* Updated on OP_MAP */
    size_t vo_len;

    /* Flags to use */
    int vo_flags;

    /* Backing handle/offset - only if VMOP_FLAG_FD is used */
    int vo_fd;
    off_t vo_offset;
};

int set_errno_or_extract_value(int v)
{
    if (v & 0x80000000) {
        errno = v & 0x1ff;
        return -1;
    }
    return v & 0x7fffffff;
}

void _exit(int n)
{
    extern void _SYS_exit(long);
    _SYS_exit(n);
    for (;;)
        __asm __volatile("ud2");
}

#define SYSCALL0(name)         \
    extern long _SYS_##name(); \
    int name(long a) { return set_errno_or_extract_value(_SYS_##name()); }

#define SYSCALL1(name)                                     \
    int name(long a)                                       \
    {                                                      \
        extern long _SYS_##name(long a);                   \
        return set_errno_or_extract_value(_SYS_##name(a)); \
    }

#define SYSCALL2(name)                                        \
    int name(long a, long b)                                  \
    {                                                         \
        extern long _SYS_##name(long a, long b);              \
        return set_errno_or_extract_value(_SYS_##name(a, b)); \
    }

#define SYSCALL3(name)                                           \
    int name(long a, long b, long c)                             \
    {                                                            \
        extern long _SYS_##name(long a, long b, long c);         \
        return set_errno_or_extract_value(_SYS_##name(a, b, c)); \
    }

#define SYSCALL4(name)                                              \
    int name(long a, long b, long c, long d)                        \
    {                                                               \
        extern long _SYS_##name(long a, long b, long c, long d);    \
        return set_errno_or_extract_value(_SYS_##name(a, b, c, d)); \
    }

#define SYSCALL5(name)                                                   \
    int name(long a, long b, long c, long d, long e)                     \
    {                                                                    \
        extern long _SYS_##name(long a, long b, long c, long d, long e); \
        return set_errno_or_extract_value(_SYS_##name(a, b, c, d, e));   \
    }

#define SYSCALL6(name)                                                           \
    int name(long a, long b, long c, long d, long e, long f)                     \
    {                                                                            \
        extern long _SYS_##name(long a, long b, long c, long d, long e, long f); \
        return set_errno_or_extract_value(_SYS_##name(a, b, c, d, e, f));        \
    }

off_t lseek(int fd, off_t offset, int whence)
{
    extern long _SYS_seek(long, long*, long);
    long l = offset;
    long r = _SYS_seek(fd, &l, whence);
    if (r < 0) {
        errno = r & 0x1ff;
        return -1;
    }
    return l;
}

static SYSCALL1(clone)

    int fork()
{
    return clone(0);
}

extern long _SYS_vmop(struct VMOP_OPTIONS*);

void* mmap(void* ptr, size_t len, int prot, int flags, int fd, off_t offset)
{
    struct VMOP_OPTIONS vo;
    memset(&vo, 0, sizeof(vo));

    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_MAP;
    vo.vo_op = OP_MAP;
    vo.vo_addr = ptr;
    vo.vo_len = len;
    if (prot & PROT_READ)
        vo.vo_flags |= VMOP_FLAG_READ;
    if (prot & PROT_WRITE)
        vo.vo_flags |= VMOP_FLAG_WRITE;
    if (prot & PROT_EXEC)
        vo.vo_flags |= VMOP_FLAG_EXECUTE;
    if (flags & MAP_FIXED)
        vo.vo_flags |= VMOP_FLAG_FIXED;
    if (flags & MAP_PRIVATE)
        vo.vo_flags |= VMOP_FLAG_PRIVATE;
    else
        vo.vo_flags |= VMOP_FLAG_SHARED;

    if (flags & MAP_ANONYMOUS) {
        vo.vo_fd = -1;
        vo.vo_offset = 0;
    } else {
        vo.vo_flags |= VMOP_FLAG_FD;
        vo.vo_fd = fd;
        vo.vo_offset = offset;
    }

    long r = _SYS_vmop(&vo);
    if (r < 0) {
        errno = r & 0x1ff;
        return MAP_FAILED;
    }

    return vo.vo_addr;
}

int munmap(void* addr, size_t len)
{
    struct VMOP_OPTIONS vo;

    memset(&vo, 0, sizeof(vo));
    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_UNMAP;
    vo.vo_addr = addr;
    vo.vo_len = len;
    return set_errno_or_extract_value(_SYS_vmop(&vo));
}

void* sbrk(intptr_t incr)
{
    struct VMOP_OPTIONS vo;

    memset(&vo, 0, sizeof(vo));
    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_SBRK;
    vo.vo_len = incr;
    long r = _SYS_vmop(&vo);
    if (r < 0) {
        errno = r & 0x1ff;
        return MAP_FAILED;
    }

    return vo.vo_addr;
}

SYSCALL4(fcntl)
SYSCALL5(ioctl)
SYSCALL4(mount)
SYSCALL4(unmount)
SYSCALL2(statfs)
SYSCALL2(fstatfs)
SYSCALL2(nanosleep)

SYSCALL3(open)
SYSCALL1(close)
SYSCALL3(read)
SYSCALL3(write)
SYSCALL1(unlink)
SYSCALL3(execve)
SYSCALL1(dup)
SYSCALL2(rename)
SYSCALL2(stat)
SYSCALL1(chdir)
SYSCALL2(fstat)
SYSCALL1(fchdir)
SYSCALL2(link)
SYSCALL2(utime)
SYSCALL2(clock_settime)
SYSCALL2(clock_gettime)
SYSCALL2(clock_getres)
SYSCALL3(readlink)
SYSCALL2(lstat)
SYSCALL2(getcwd)
SYSCALL3(sigaction)
SYSCALL3(sigprocmask)
SYSCALL1(sigsuspend)
SYSCALL2(kill)
SYSCALL0(getpgrp)
SYSCALL0(setsid)
SYSCALL2(dup2)
SYSCALL1(getsid)
SYSCALL0(getuid)
SYSCALL0(geteuid)
SYSCALL0(getgid)
SYSCALL0(getegid)
SYSCALL0(getpid)
SYSCALL0(getppid)
SYSCALL2(symlink)
SYSCALL1(reboot)
SYSCALL3(waitpid)

int pipe(int* fd)
{
    errno = ENOSYS;
    return -1;
}

int umask(int mask)
{
    errno = ENOSYS;
    return -1;
}

int killpg(pid_t pgrp, int sig)
{
    errno = ENOSYS;
    return -1;
}

int getgroups(int gidsetsize, gid_t* grouplist)
{
    if (gidsetsize > 0)
        grouplist[0] = 0;
    return 1;
}

int wait3(int* status, int options, struct rusage* rusage)
{
    if (rusage != NULL)
        memset(rusage, 0, sizeof(*rusage));
    return waitpid(0, (long*)&status, options);
}
