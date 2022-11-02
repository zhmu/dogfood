#include <sys/types.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/times.h>
#include <sys/utime.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <grp.h>
#include <pwd.h>
#include <dogfood/fcntl.h>
#include <dogfood/utsname.h>
#include <dogfood/vmop.h>

extern char** environ;

static long set_errno_or_extract_value(long v)
{
    if (v < 0) {
        errno = -v;
        return -1;
    }
    return v;
}

void _exit(int n)
{
    extern void _SYS_exit(long);
    _SYS_exit(n);
    for (;;)
        __asm __volatile("ud2");
}

#define SYSCALL0(name, rt)     \
    extern long _SYS_##name(); \
    rt name() { return (rt)set_errno_or_extract_value(_SYS_##name()); }

#define SYSCALL1(name, rt, t1)                          \
    rt name(t1 a)                                       \
    {                                                      \
        extern long _SYS_##name(long a);                   \
        return (rt)set_errno_or_extract_value(_SYS_##name((long)a)); \
    }

#define SYSCALL2(name, rt, t1, t2)                                        \
    rt name(t1 a, t2 b)                                  \
    {                                                         \
        extern long _SYS_##name(long a, long b);              \
        return (rt)set_errno_or_extract_value(_SYS_##name((long)a, (long)b)); \
    }

#define SYSCALL3(name, rt, t1, t2, t3)                           \
    rt name(t1 a, t2 b, t3 c)                             \
    {                                                            \
        extern long _SYS_##name(long a, long b, long c);         \
        return (rt)set_errno_or_extract_value(_SYS_##name((long)a, (long)b, (long)c)); \
    }

#define SYSCALL4(name, rt, t1, t2, t3, t4)                          \
    rt name(t1 a, t2 b, t3 c, t4 d)                        \
    {                                                               \
        extern long _SYS_##name(long a, long b, long c, long d);    \
        return (rt)set_errno_or_extract_value(_SYS_##name((long)a, (long)b, (long)c, (long)d)); \
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
        errno = -r;
        return -1;
    }
    return l;
}

SYSCALL1(clone, int, int)

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
        errno = -r;
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

extern long _SYS_getcwd(char*, int);

char* getcwd(char* buf, size_t size)
{
    long r = _SYS_getcwd(buf, (int)size);
    if (r == 0)
        return buf;
    errno = -r;
    return NULL;
}

SYSCALL4(fcntl, int, int, long, long, long)
SYSCALL5(ioctl)
SYSCALL4(mount, int, const char*, const char*, const char*, long)
SYSCALL1(unmount, int, const char*)
SYSCALL2(statfs, int, const char*, struct statfs*)
SYSCALL2(fstatfs, int, int, struct statfs*)
SYSCALL2(nanosleep, int, const struct timespec*, struct timespec*)

SYSCALL3(open, int, const char*, int, mode_t)
SYSCALL1(close, int, int)
SYSCALL3(read, ssize_t, int, void*, size_t)
SYSCALL3(write, ssize_t, int, void*, size_t)
SYSCALL1(unlink, int, const char*)
SYSCALL3(execve, int, const char*, const char**, const char**)
SYSCALL1(dup, int, int)
SYSCALL1(uname, int, struct utsname*)
SYSCALL1(chdir, int, const char*)
SYSCALL2(fstat, int, int, struct stat*)
SYSCALL1(fchdir,int,  int)
SYSCALL2(link, int, const char*, const char*)
SYSCALL2(utime, int, const char*, const struct utimbuf*)
SYSCALL2(clock_settime, int, clockid_t, struct timespec*)
SYSCALL2(clock_gettime, int, clockid_t, const struct timespec*)
SYSCALL2(clock_getres, int, clockid_t, struct timespec*)
SYSCALL3(readlink, ssize_t, const char*, char*, size_t)
SYSCALL3(sigprocmask, int, int, const sigset_t*, sigset_t*)
SYSCALL1(sigsuspend, int, const sigset_t*)
SYSCALL2(kill, int, pid_t, int)
SYSCALL0(getpgrp, int)
SYSCALL0(setsid, pid_t)
SYSCALL2(dup2, int, int, int)
SYSCALL1(getsid, pid_t, pid_t)
SYSCALL0(getuid, uid_t)
SYSCALL0(geteuid, uid_t)
SYSCALL0(getgid, gid_t)
SYSCALL0(getegid, gid_t)
SYSCALL0(getpid, pid_t)
SYSCALL0(getppid, pid_t)
SYSCALL2(symlink, int, const char*, const char*)
SYSCALL1(reboot, int, int)
SYSCALL3(waitpid, pid_t, pid_t, int*, int)
SYSCALL3(chown, int, const char*, uid_t, gid_t)
SYSCALL3(fchown, int, int, uid_t, gid_t)
SYSCALL1(umask, mode_t, mode_t)
SYSCALL2(chmod, int, const char*, mode_t)
SYSCALL2(mkdir, int, const char*, mode_t)
SYSCALL1(rmdir, int, const char*)
SYSCALL2(fchmod, int, int, mode_t)
SYSCALL4(fstatat, int, int, const char*, struct stat*, int)
SYSCALL4(ptrace, long, int, pid_t, void*, void*)

int pipe(int* fd)
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
    return waitpid(0, status, options);
}

int getmntinfo(struct statfs** mntbufp, int mode)
{
    errno = ENOSYS;
    return -1;
}

int fsync(int fd) { return 0; }

struct servent* getservbyname(const char* name, const char* proto) { return NULL; }

int setpgid(pid_t pid, pid_t pgid)
{
    errno = -ENOSYS;
    return -1;
}

unsigned int alarm(unsigned int seconds) { return 0; }

int setreuid(uid_t ruid, uid_t euid)
{
    errno = -ENOSYS;
    return -1;
}

int setregid(gid_t rgid, gid_t egid)
{
    errno = -ENOSYS;
    return -1;
}

#define _SC_PAGESIZE 8
#define _SC_CLK_TCK 2

long sysconf(int name)
{
    switch (name) {
        case _SC_PAGESIZE:
            return 4096;
        case _SC_CLK_TCK:
            return 100;
    }
    return 0;
}

struct group* getgrnam(const char* name) { return NULL; }

struct group* getgrgid(gid_t gid) { return NULL; }

int ftruncate(int fd, off_t length)
{
    errno = ENOSYS;
    return -1;
}

int gettimeofday(struct timeval* tv, void* __tz)
{
    errno = ENOSYS;
    return -1;
}

int settimeofday(const struct timeval* tv, const struct timezone* tz)
{
    errno = ENOSYS;
    return -1;
}

long fpathconf(int fd, int name)
{
    errno = ENOSYS;
    return -1;
}

long pathconf(const char* path, int name)
{
    errno = ENOSYS;
    return -1;
}

struct group* getgrent(void) { return NULL; }

void setgrent(void) {}

void endgrent(void) {}

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout)
{
    errno = ENOSYS;
    return -1;
}

int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timespec* timeout, const sigset_t* set)
{
    errno = ENOSYS;
    return -1;
}


int getrlimit(int resource, struct rlimit* rlim)
{
    errno = ENOSYS;
    return -1;
}

char* ttyname(int fd) { return "console"; }

clock_t times(struct tms* buf)
{
    errno = ENOSYS;
    return (clock_t)-1;
}

int setrlimit(int resource, const struct rlimit* rlim)
{
    errno = ENOSYS;
    return -1;
}

int accept(int socket, struct sockaddr* address, socklen_t* address_len)
{
    errno = ENOSYS;
    return -1;
}

int bind(int socket, const struct sockaddr* address, socklen_t address_len)
{
    errno = ENOSYS;
    return -1;
}

int connect(int socket, const struct sockaddr* address, socklen_t address_len)
{
    errno = ENOSYS;
    return -1;
}

int listen(int socket, int backlog)
{
    errno = ENOSYS;
    return -1;
}

int socket(int domain, int type, int protocol)
{
    errno = ENOSYS;
    return -1;
}

ssize_t send(int socket, const void* buffer, size_t length, int flags)
{
    errno = ENOSYS;
    return -1;
}

unsigned int sleep(unsigned int seconds) { return 0; }

int wait(int* wstatus) { return waitpid(-1, wstatus, 0); }

int stat(const char* path, struct stat* st)
{
    return fstatat(AT_FDCWD, path, st, 0);
}

int lstat(const char* path, struct stat* st)
{
    return fstatat(AT_FDCWD, path, st, AT_SYMLINK_NOFOLLOW);
}


extern long _SYS_sigaction(long a, long b, long c);
extern long _SYS_sigreturn();

int sigaction(int sig, const struct sigaction* act, struct sigaction* oact)
{
    struct sigaction newact;
    if (act != NULL) {
        newact = *act;
        newact.sa_restorer = (void(*)(void))&_SYS_sigreturn;
    }
    const int r = _SYS_sigaction(sig, act != NULL ? (long)&newact : 0, (long)oact);
    return set_errno_or_extract_value(r);
}

sig_t signal(int sig, sig_t func)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    struct sigaction oact;
    if (sigaction(sig, &act, &oact) < 0)
        return SIG_ERR;
    return oact.sa_handler;
}

int raise(int sig)
{
    return kill(getpid(), sig);
}

static inline unsigned int signal_mask(int signo)
{
    if (signo < SIGHUP || signo >= NSIG)
        return 0;
    return 1 << (signo - 1);
}

int sigemptyset(sigset_t* set)
{
    *set = 0;
    return 0;
}

int sigfillset(sigset_t* set)
{
    int result = sigemptyset(set);
    for (unsigned int signo = 1; result == 0 && signo < NSIG; signo++)
        result = sigaddset(set, signo);
    return result;
}

int sigaddset(sigset_t* set, int signo)
{
    unsigned int mask = signal_mask(signo);
    if (mask == 0) {
        errno = EINVAL;
        return -1;
    }
    *set |= mask;
    return 0;
}

int sigdelset(sigset_t* set, int signo)
{
    unsigned int mask = signal_mask(signo);
    if (mask == 0) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~mask;
    return 0;
}

int sigismember(const sigset_t* set, int signo)
{
    unsigned int mask = signal_mask(signo);
    if (mask == 0) {
        errno = EINVAL;
        return -1;
    }
    return (*set & mask) ? 1 : 0;
}
