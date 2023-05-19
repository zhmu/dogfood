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
    result::MaybeInt PTrace(amd64::TrapFrame& tf)
    {
        const auto req = syscall::GetArgument<1, int>(tf);

        if (req == PTRACE_TRACEME) {
            auto& current = process::GetCurrent();
            if (current.ptrace.traced) return result::Error(error::Code::PermissionDenied);
            current.ptrace.traced = true;
            return 0;
        }

        const auto pid = syscall::GetArgument<2, pid_t>(tf);
        auto proc = process::FindProcessByPID(pid);
        if (!proc) return result::Error(error::Code::NotFound);

        if (req == PTRACE_ATTACH) {
            auto& current = process::GetCurrent();
            if (&current == proc) return result::Error(error::Code::PermissionDenied); // can't trace self
            if (proc->ptrace.traced) return result::Error(error::Code::PermissionDenied); // already traced
            // Change parent to the current process (tracer); upon ptrace
            // detach/exit, we'll revert to the original parent
            proc->ptrace.traced = true;
            proc->parent = &current;
            return 0;
        }

        if (!proc->ptrace.traced) return result::Error(error::Code::NotFound);
        if (proc->state != process::State::Stopped) return result::Error(error::Code::NotFound);

        //auto addr = syscall::GetArgument<3, uint8_t*>(tf);
        switch(req) {
            case PTRACE_DETACH:
                proc->ptrace.traced = false;
                proc->ptrace.traceSyscall = false;
                proc->ptrace.traceFork = false;
                proc->parent = proc->real_parent; // TODO this could be nullptr?
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
            case PTRACE_SETOPTIONS: {
                auto dataPtr = syscall::GetArgument<4, char*>(tf);
                uint64_t data = reinterpret_cast<uint64_t>(dataPtr.get());
                proc->ptrace.traceFork = (data & PTRACE_O_TRACEFORK) != 0;
                return 0;
            }
            case PTRACE_CONT: {
                auto dataPtr = syscall::GetArgument<4, char*>(tf);
                uint64_t data = reinterpret_cast<uint64_t>(dataPtr.get());
                proc->ptrace.signal = data;
                proc->state = process::State::Runnable;
                return 0;
            }
            case PTRACE_PEEK:
                return result::Error(error::Code::InvalidArgument);

        }
        return result::Error(error::Code::InvalidArgument);
    }
}
