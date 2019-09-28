#include "types.h"
#include "syscall.h"
#include "amd64.h"
#include "console.h"
#include "exec.h"
#include "process.h"
#include "lib.h"

extern "C" uint64_t syscall(amd64::Syscall* sc)
{
    switch (sc->no) {
        case SYS_write: {
            printf("write: %d %p %x\n", sc->arg1, sc->arg2, sc->arg3);
            auto s = reinterpret_cast<const char*>(sc->arg2);
            auto len = sc->arg3;
            while (len--)
                console::put_char(*s++);
            return -1;
        }
        case SYS_execve:
            return exec(*sc);
        case SYS_getsid:
        case SYS_getuid:
        case SYS_geteuid:
        case SYS_getgid:
        case SYS_getegid:
            return 0; // not implemented
        case SYS_getpid:
        case SYS_getppid: // not implemented
            return process::GetCurrent().pid;
    }
    printf(
        "unsupported syscall %d %lx [%x %x %x %x %x %x]\n", sc->no, sc->rip, sc->arg1, sc->arg2,
        sc->arg3, sc->arg4, sc->arg5, sc->arg6);
    return -1;
}
