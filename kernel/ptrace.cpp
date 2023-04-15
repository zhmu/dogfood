#include "ptrace.h"
#include "syscall.h"
#include "process.h"
#include <dogfood/ptrace.h>
#include <dogfood/user.h>
#include "lib.h"

/*
 * ptrace relies heavily on the UNIX signal delivery mechanism to convey
 * information between the tracer and the tracee.
 *
 * The flow is the following:
 *
 * strace             child
 *    |-----fork-------->
 *    | waitpid()       |
 *    |                 | PTRACE_TRACEME
 *    |                 |
 *    |                 | raise(SIGSTOP) / execve()
 *    |<-----wakes------|
 *    |                 |
 *    | PTRACE_SYSCALL--> [runs until syscall start]
 *    | waitpid()       |
 *    |                 |
 *    |<-----wakes------|
 *    | PTRACE_GETREGS->|
 *    | PTRACE_SYSCALL->| [resumes until syscall end]
 *    | waitpid()       |
 *    |<-----wakes------|
 *    |                 |
 *    | PTRACE_GETREGS  |
 *    | PTRACE_SYSCALL  | [resumes]
 *   ...
 *
 *  A series of PTRACE_GETREGS, PTRACE_SYSCALL, PTRACE_GETREGS, PTRACE_SYSCALL
 *  constitutes one completed system call on the child (the tracee)
 *
 *  Typical ptrace() has support for debugging and changing/avoiding
 *  signals/syscalls but this doesn't implement that yet.
 */
namespace ptrace
{
    std::expected<int, error::Code> PTrace(amd64::TrapFrame& tf)
    {
        const auto req = syscall::GetArgument<1, int>(tf);

        if (req == PTRACE_TRACEME) {
            auto& current = process::GetCurrent();
            if (current.ptrace.traced) return std::unexpected(error::Code::PermissionDenied);
            current.ptrace.traced = true;
            return 0;
        }

        const auto pid = syscall::GetArgument<2, pid_t>(tf);
        auto proc = process::FindProcessByPID(pid);
        if (!proc) return std::unexpected(error::Code::NotFound);

        if (req == PTRACE_ATTACH) {
            auto& current = process::GetCurrent();
            if (&current == proc) return std::unexpected(error::Code::PermissionDenied); // can't trace self
            // TODO: maybe change parent?
            proc->ptrace.traced = true;
            return 0;
        }

        if (!proc->ptrace.traced) return std::unexpected(error::Code::NotFound);
        if (proc->state != process::State::Stopped) return std::unexpected(error::Code::NotFound);

        //auto addr = syscall::GetArgument<3, uint8_t*>(tf);
        switch(req) {
            case PTRACE_DETACH:
                proc->ptrace.traced = false;
                proc->ptrace.traceSyscall = false;
                break;
            case PTRACE_SYSCALL:
                proc->ptrace.traceSyscall = true;
                proc->state = process::State::Runnable;
                return 0;
            case PTRACE_GETREGS: {
                auto regsPtr = syscall::GetArgument<4, user_registers*>(tf);
                struct user_registers ur{};
                ur.rax = proc->trapFrame->rax;
                ur.rbx = proc->trapFrame->rbx;
                ur.rcx = proc->trapFrame->rcx;
                ur.rdx = proc->trapFrame->rdx;
                ur.rbp = proc->trapFrame->rbp;
                ur.rsi = proc->trapFrame->rsi;
                ur.rdi = proc->trapFrame->rdi;
                ur.r8 = proc->trapFrame->r8;
                ur.r9 = proc->trapFrame->r9;
                ur.r10 = proc->trapFrame->r10;
                ur.r11 = proc->trapFrame->r11;
                ur.r12 = proc->trapFrame->r12;
                ur.r13 = proc->trapFrame->r13;
                ur.r14 = proc->trapFrame->r14;
                ur.r15 = proc->trapFrame->r15;
                ur.rip = proc->trapFrame->rip;
                ur.rflags = proc->trapFrame->rflags;
                ur.rsp = proc->trapFrame->rsp;
                ur.cs = proc->trapFrame->cs;
                ur.ss = proc->trapFrame->ss;
                // We don't save these registers as they do not change
                ur.ds = static_cast<uint16_t>(amd64::Selector::UserData) + 3;
                ur.es = static_cast<uint16_t>(amd64::Selector::UserData) + 3;
                ur.fs = 0;
                ur.gs = 0;
                return regsPtr.Set(ur);
            }
            case PTRACE_PEEK:
                return std::unexpected(error::Code::InvalidArgument);

        }
        return std::unexpected(error::Code::InvalidArgument);
    }
}
