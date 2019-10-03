#define SYS_exit 0
// void sys_exit(int exitcode);
#define SYS_read 1
// Result sys_read(fdindex_t fd, void* buf, size_t len);
#define SYS_write 2
// Result sys_write(fdindex_t fd, const void* buf, size_t len);
#define SYS_open 3
// Result sys_open(const char* path, int flags, int mode);
#define SYS_close 4
// Result sys_close(fdindex_t fd);
#define SYS_unlink 5
// Result sys_unlink(const char* path);
#define SYS_seek 6
// Result sys_seek(fdindex_t fd, off_t* offset, int whence);
#define SYS_clone 7
// Result sys_clone(int flags);
#define SYS_waitpid 8
// Result sys_waitpid(pid_t pid, int* stat_loc, int options);
#define SYS_execve 9
// Result sys_execve(const char* path, const char** argv, const char** envp);
#define SYS_vmop 10
// Result sys_vmop(struct VMOP_OPTIONS* opts);
#define SYS_dup 11
// Result sys_dup(fdindex_t fd);
#define SYS_rename 12
// Result sys_rename(const char* oldpath, const char* newpath);
#define SYS_stat 13
// Result sys_stat(const char* path, struct stat* buf);
#define SYS_chdir 14
// Result sys_chdir(const char* path);
#define SYS_fstat 15
// Result sys_fstat(fdindex_t index, struct stat* buf);
#define SYS_fchdir 16
// Result sys_fchdir(fdindex_t index);
#define SYS_fcntl 17
// Result sys_fcntl(fdindex_t index, int cmd, const void* in, void* out);
#define SYS_link 18
// Result sys_link(const char* oldpath, const char* newpath);
#define SYS_utime 19
// Result sys_utime(const char* path, const struct utimbuf* times);
#define SYS_clock_settime 20
// Result sys_clock_settime(clockid_t id, const struct timespec* tp);
#define SYS_clock_gettime 21
// Result sys_clock_gettime(clockid_t id, struct timespec* tp);
#define SYS_clock_getres 22
// Result sys_clock_getres(clockid_t id, struct timespec* res);
#define SYS_readlink 23
// Result sys_readlink(const char* path, char* buffer, size_t bufsize);
#define SYS_lstat 24
// Result sys_lstat(const char* path, struct stat* buf);
#define SYS_getcwd 25
// Result sys_getcwd(char* buf, size_t buflen);
#define SYS_sigaction 26
// Result sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oact);
#define SYS_sigprocmask 27
// Result sys_sigprocmask(int how, const sigset_t* set, sigset_t* oset);
#define SYS_sigsuspend 28
// Result sys_sigsuspend(const sigset_t* sigmask);
#define SYS_kill 29
// Result sys_kill(pid_t pid, int sig);
#define SYS_sigreturn 30
// Result sys_sigreturn();
#define SYS_ioctl 31
// Result sys_ioctl(fdindex_t fd, unsigned long op, void* arg1, void* arg2, void* arg3);
#define SYS_getpgrp 32
// Result sys_getpgrp();
#define SYS_setpgid 33
// Result sys_setpgid(pid_t pid, pid_t pgid);
#define SYS_setsid 34
// Result sys_setsid();
#define SYS_dup2 35
// Result sys_dup2(fdindex_t fd, fdindex_t newindex);
#define SYS_mount 36
// Result sys_mount(const char* type, const char* source, const char* dir, int flags);
#define SYS_unmount 37
// Result sys_unmount(const char* dir, int flags);
#define SYS_statfs 38
// Result sys_statfs(const char* path, struct statfs* buf);
#define SYS_fstatfs 39
// Result sys_fstatfs(fdindex_t fd, struct statfs* buf);
#define SYS_nanosleep 40
// Result sys_nanosleep(const struct timespec* rqtp, struct timespec* rmtp);
#define SYS_getsid 41
// Result sys_getsid(pid_t pid);
#define SYS_getuid 42
// Result sys_getuid();
#define SYS_geteuid 43
// Result sys_geteuid();
#define SYS_getgid 44
// Result sys_getgid();
#define SYS_getegid 45
// Result sys_getegid();
#define SYS_getpid 46
// Result sys_getpid();
#define SYS_getppid 47
// Result sys_getppid();
#define SYS_symlink 48
// Result sys_symlink(const char* oldpath, const char* newpath);
#define SYS_reboot 49
// Result sys_reboot(int how);

#ifdef __cplusplus
#include "types.h"
#include "amd64.h"

namespace syscall
{
    template<size_t N>
    uint64_t GetArgument(amd64::TrapFrame&);

    inline uint64_t GetNumber(amd64::TrapFrame& tf) { return tf.rax; }
    template<>
    inline uint64_t GetArgument<1>(amd64::TrapFrame& tf)
    {
        return tf.rdi;
    }
    template<>
    inline uint64_t GetArgument<2>(amd64::TrapFrame& tf)
    {
        return tf.rsi;
    }
    template<>
    inline uint64_t GetArgument<3>(amd64::TrapFrame& tf)
    {
        return tf.rdx;
    }
    template<>
    inline uint64_t GetArgument<4>(amd64::TrapFrame& tf)
    {
        return tf.r10;
    }
    template<>
    inline uint64_t GetArgument<5>(amd64::TrapFrame& tf)
    {
        return tf.r8;
    }
    template<>
    inline uint64_t GetArgument<6>(amd64::TrapFrame& tf)
    {
        return tf.r9;
    }
} // namespace syscall
#endif
