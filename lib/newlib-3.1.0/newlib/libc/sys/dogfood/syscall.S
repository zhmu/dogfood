#include "dogfood/syscall.h"

.text

1:
    movq    %rcx, %r10
    syscall
    ret

#define SYSCALL(n) \
.globl _SYS_##n; \
_SYS_##n: \
    movq    $SYS_##n, %rax; \
    jmp     1b

SYSCALL(exit)
SYSCALL(read)
SYSCALL(write)
SYSCALL(open)
SYSCALL(close)
SYSCALL(unlink)
SYSCALL(seek)
SYSCALL(clone)
SYSCALL(waitpid)
SYSCALL(execve)
SYSCALL(vmop)
SYSCALL(dup)
SYSCALL(stat)
SYSCALL(chdir)
SYSCALL(fstat)
SYSCALL(fchdir)
SYSCALL(fcntl)
SYSCALL(link)
SYSCALL(utime)
SYSCALL(clock_settime)
SYSCALL(clock_gettime)
SYSCALL(clock_getres)
SYSCALL(readlink)
SYSCALL(lstat)
SYSCALL(getcwd)
SYSCALL(sigaction)
SYSCALL(sigprocmask)
SYSCALL(sigsuspend)
SYSCALL(kill)
SYSCALL(sigreturn)
SYSCALL(ioctl)
SYSCALL(getpgrp)
SYSCALL(setpgid)
SYSCALL(setsid)
SYSCALL(dup2)
SYSCALL(mount)
SYSCALL(unmount)
SYSCALL(statfs)
SYSCALL(fstatfs)
SYSCALL(nanosleep)
SYSCALL(getsid)
SYSCALL(getuid)
SYSCALL(geteuid)
SYSCALL(getgid)
SYSCALL(getegid)
SYSCALL(getpid)
SYSCALL(getppid)
SYSCALL(symlink)
SYSCALL(reboot)
SYSCALL(chown)
SYSCALL(fchown)
SYSCALL(umask)
SYSCALL(chmod)
SYSCALL(mkdir)
SYSCALL(rmdir)
SYSCALL(fchmod)
