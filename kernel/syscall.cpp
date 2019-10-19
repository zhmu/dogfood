#include "types.h"
#include "syscall.h"
#include "amd64.h"
#include "console.h"
#include "exec.h"
#include "file.h"
#include "errno.h"
#include "process.h"
#include "lib.h"
#include "vm.h"

extern "C" uint64_t perform_syscall(amd64::TrapFrame* tf)
{
    switch (syscall::GetNumber(*tf)) {
        case SYS_exit:
            return process::Exit(*tf);
        case SYS_write: {
            auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
            if (file == nullptr) return -EBADF;
            auto buf = reinterpret_cast<const void*>(syscall::GetArgument<2>(*tf));
            auto len = syscall::GetArgument<3>(*tf);
            return file::Write(*file, buf, len);
        }
        case SYS_read: {
            auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
            if (file == nullptr) return -EBADF;
            auto buf = reinterpret_cast<void*>(syscall::GetArgument<2>(*tf));
            auto len = syscall::GetArgument<3>(*tf);
            return file::Read(*file, buf, len);
        }
        case SYS_vmop:
            return vm::VmOp(*tf);
        case SYS_kill:
            return process::Kill(*tf);
        case SYS_clone:
            return process::Fork(*tf);
        case SYS_waitpid:
            return process::WaitPID(*tf);
        case SYS_execve:
            return exec(*tf);
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
        "[%d] unsupported syscall %d %lx [%x %x %x %x %x %x]\n", process::GetCurrent().pid,
        syscall::GetNumber(*tf), syscall::GetArgument<1>(*tf), syscall::GetArgument<2>(*tf),
        syscall::GetArgument<3>(*tf), syscall::GetArgument<4>(*tf), syscall::GetArgument<5>(*tf),
        syscall::GetArgument<6>(*tf));
    return -1;
}
