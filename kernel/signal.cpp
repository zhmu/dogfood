#include "x86_64/amd64.h"
#include "lib.h"
#include "process.h"
#include "syscall.h"
#include <dogfood/errno.h>
#include <dogfood/signal.h>
#include <bit>
#include <limits>
#include <utility>
#include "debug.h"
#include "error.h"

/*
 * POSIX-like signal processing: the idea is that we'll deliver any pending
 * signals whenever we are returning from a system call (syscall_handler in
 * exception.S will call deliver_signal().
 *
 * The tricky case is delivering signals to userland. The scenario is the
 * following:
 *
 *     int main() {
 *         signal(SIGHUP, handler);
 *         raise(SIGHUP);      ---> (a) syscall completion calls handler()
 *     2:
 *         ...
 *     }
 *
 *     void handler(int signum) {
 *        ....
 *     } --> (b) return to 2 using sigreturn syscall
 *
 *
 * Our approach mimics the approach taken by Linux: deliver_signal() will
 * create a new amd64::TrapFrame on the kernel stack and use this frame to
 * enter handler() with the correct parameters. Hence, deliver_signal() returns
 * the address of the TrapFrame it wants to be restored.
 *
 * In order to properly return from handler()'s end to 2, we use 'sigreturn'
 * system call, which restores the original TrapFrame used to enter the kernel
 * (a).
 *
 * This still poses two challenges:
 *
 * - How does handler() invoke sigreturn() upon completion?
 *   We take the same approach as Linux: sigaction() tells the kernel where to
 *   continue execution once the signal handler ends using the sa_restorer
 *   member of sigaction. This is the return address used for the call to handler()
 *
 * - How do we ensure the original trap frame isn't overwritten when we invoke
 *   syscalls in handler()?
 *   We avoid this by changing the process' kernel stack (rsp0) once we're
 *   delivering a signal. This means we'll leave the first TrapFrame bytes
 *   as they were, which ensures sigreturn() can properly restore them.
 *
 * TODO:
 *
 * - Unblock process upon signal delivery
 * - Nested signals
 * - Masking
 */

namespace signal {
    namespace
    {
        constexpr debug::Trace<false> Debug;

        std::optional<int> SignalNumberToIndex(const int sig)
        {
            if (sig >= 1 && sig < NSIG) return sig - 1;
            return {};
        }

        template<typename T>
        std::optional<int> ExtractPendingSignalBit(T& pending)
        {
            const auto value = pending.to_ullong();
            if (!value) return {};

            const auto bits = std::numeric_limits<decltype(value)>::digits;
            const auto bit = (bits - 1) - std::countl_zero(value);
            return bit;
        }

        template<typename T>
        std::optional<int> ExtractAndResetPendingSignal(T& pending)
        {
            const auto bit = ExtractPendingSignalBit(pending);
            if (!bit) return {};

            pending.reset(*bit);
            return *bit + 1;
        }

        enum class DefaultAction {
            Terminate,
            CoreDump,
            Ignore,
            Stop,
            Continue,
        };

        DefaultAction GetSignalDefaultAction(const int signo)
        {
            switch(signo) {
                case SIGHUP: return DefaultAction::Terminate;
                case SIGINT: return DefaultAction::Terminate;
                case SIGQUIT: return DefaultAction::CoreDump;
                case SIGILL: return DefaultAction::CoreDump;
                case SIGTRAP: return DefaultAction::CoreDump;
                case SIGABRT: return DefaultAction::CoreDump;
                case SIGBUS: return DefaultAction::CoreDump;
                case SIGFPE: return DefaultAction::CoreDump;
                case SIGKILL: return DefaultAction::Terminate;
                case SIGUSR1: return DefaultAction::Terminate;
                case SIGSEGV: return DefaultAction::CoreDump;
                case SIGUSR2: return DefaultAction::Terminate;
                case SIGPIPE: return DefaultAction::Terminate;
                case SIGALRM: return DefaultAction::Terminate;
                case SIGTERM: return DefaultAction::Terminate;
                case SIGCHLD: return DefaultAction::Ignore;
                case SIGCONT: return DefaultAction::Continue;
                case SIGSTOP: return DefaultAction::Stop;
                case SIGTSTP: return DefaultAction::Stop;
                case SIGTTIN: return DefaultAction::Stop;
                case SIGTTOU: return DefaultAction::Stop;
                case SIGURG: return DefaultAction::Ignore;
                case SIGXCPU: return DefaultAction::CoreDump;
                case SIGXFSZ: return DefaultAction::CoreDump;
                case SIGVTALRM: return DefaultAction::Terminate;
                case SIGPROF: return DefaultAction::Terminate;
                case SIGSYS: return DefaultAction::Continue;
            }
            return DefaultAction::Terminate;
        }
    }

    Action::Action(const struct sigaction& sa)
        : mask(sa.sa_mask), flags(sa.sa_flags), restorer(sa.sa_restorer)
    {
        if (sa.sa_flags & SA_SIGINFO)
            handler = reinterpret_cast<Handler>(sa.sa_sigaction);
        else
            handler = sa.sa_handler;
    }

    struct sigaction Action::ToSigAction() const
    {
        struct sigaction sa{};
        sa.sa_mask = mask;
        sa.sa_flags = flags;
        sa.sa_restorer = restorer;
        if (flags & SA_SIGINFO)
            sa.sa_sigaction = reinterpret_cast<void(*)(int, siginfo_t*, void*)>(handler);
        else
            sa.sa_handler = handler;
        return sa;
    }

    bool Send(process::Process& proc, int signal)
    {
        const auto index = SignalNumberToIndex(signal);
        if (!index)
            return false;

        proc.signal.pending.set(*index);

        // Wake up process upon new signal retrieval
        auto state = interrupts::SaveAndDisable();
        if (proc.state == process::State::Sleeping) {
            proc.state = process::State::Runnable;
        }
        interrupts::Restore(state);

        // Syscall return will handle the signal, via deliver_signal()
        // TODO should we also handle it in interrupts?
        return true;
    }

    bool HasPending(process::Process& proc)
    {
        return ExtractPendingSignalBit(proc.signal.pending).has_value();
    }

    result::MaybeInt kill(amd64::TrapFrame& tf)
    {
        const auto pid = syscall::GetArgument<1, int>(tf);
        const auto signal = syscall::GetArgument<2, int>(tf);
        if (pid < 0)
            return result::Error(error::Code::PermissionDenied);
        const auto index = SignalNumberToIndex(signal);
        if (!index)
            return result::Error(error::Code::InvalidArgument);

        auto proc = process::FindProcessByPID(pid);
        if (proc == nullptr)
            return result::Error(error::Code::NotFound);

        if (Send(*proc, signal))
            return 0;
        return result::Error(error::Code::InvalidArgument);
    }

    result::MaybeInt sigaction(amd64::TrapFrame& tf)
    {
        const auto signum = syscall::GetArgument<1>(tf);
        const auto act = syscall::GetArgument<2, struct sigaction*>(tf);
        auto oldact = syscall::GetArgument<3, struct sigaction*>(tf);

        Debug("sigaction ", signum, " act ", act.get(), " oldact ", oldact.get(), "\n");

        const auto index = SignalNumberToIndex(signum);
        if (!index)
            return result::Error(error::Code::InvalidArgument);

        auto& action = process::GetCurrent().signal.action[*index];
        if (oldact && !oldact.Set(action.ToSigAction()))
            return result::Error(error::Code::MemoryFault);

        if (act) {
            const auto new_action = *act;
            if (!new_action) return result::Error(error::Code::MemoryFault);
            action = *new_action;
        }
        return 0;
    }

    result::MaybeInt sigprocmask(amd64::TrapFrame& tf)
    {
        const auto how = syscall::GetArgument<1>(tf);
        auto set = syscall::GetArgument<2, sigset_t*>(tf);
        auto oset = syscall::GetArgument<3, sigset_t*>(tf);
        auto& mask = process::GetCurrent().signal.mask;

        if (oset && !oset.Set(mask)) return result::Error(error::Code::MemoryFault);

        switch(how) {
            case SIG_BLOCK:
                if (set) mask = mask | **set;
                // TODO handle signals that can't be blocked
                break;
            case SIG_UNBLOCK:
                if (set) mask = mask & ~(**set);
                break;
            case SIG_SETMASK:
                mask = set;
                break;
            default:
                return result::Error(error::Code::InvalidArgument);
        }

        return 0;
    }

    result::MaybeInt sigreturn(amd64::TrapFrame& tf)
    {
        auto& proc = process::GetCurrent();
        Debug(">> sigreturn: rsp ", &tf, " proc.TrapFrame ", proc.trapFrame, "\n");

        // Adjust rsp0 back so that the previous frame will be overwritten
        proc.rsp0 += sizeof(amd64::TrapFrame);
        process::UpdateKernelStackForProcess(proc);

        // And overwrite our current trapframe with the original one, so we'll return to the pre-signal
        // handling spot
        auto preSignalTF = proc.trapFrame;
        Debug("preSignalTF rsp ", print::Hex{preSignalTF->rsp}, " rip ", print::Hex{preSignalTF->rip}, "\n");
        tf = *preSignalTF;
        return 0;
    }

    amd64::TrapFrame& DeliverSignal(amd64::TrapFrame& tf, amd64::TrapFrame& newTF)
    {
        auto& proc = process::GetCurrent();
        while(true) {
            const auto pending_signal = ExtractAndResetPendingSignal(proc.signal.pending);
            if (!pending_signal) break;
            int signo = *pending_signal;
            Debug("DeliverSignal(", proc.pid, "): delivering pending signal ", signo, "\n");

            if (proc.ptrace.traced && signo != SIGKILL) {
                // Ask the debugger what is to be done with the signal
                Debug("DeliverSignal(", proc.pid, ", ", signo, "): ptrace'd, relaying to parent ", proc.parent->pid, "\n");
                proc.ptrace.signal = signo;
                proc.state = process::State::Stopped;
                signal::Send(*proc.parent, SIGCHLD);
                process::Yield();
                signo = std::exchange(proc.ptrace.signal, 0);
                Debug("DeliverSignal(", proc.pid, "): ptrace'd, back from yield, signo is now ", signo, "\n");
                if (!signo) continue;

                if (signo == SIGSTOP) continue; // ignore SIGSTOP
            }

            const auto index = SignalNumberToIndex(signo);
            assert(index);
            const auto action = proc.signal.action[*index];
            Debug("DeliverSignal(", proc.pid, ", ", signo, "): action flags ", action.flags, " mask ", action.mask, " handler ", action.handler, "\n");
            if (signo != SIGKILL && action.handler == SIG_IGN) {
                continue;
            }

            if (action.handler == SIG_DFL) {
                switch(GetSignalDefaultAction(signo)) {
                    case DefaultAction::CoreDump:
                    case DefaultAction::Terminate:
                        process::Exit(tf);
                        break;
                    case DefaultAction::Ignore:
                        break;
                    case DefaultAction::Stop:
                        proc.state = process::State::Stopped;
                        process::Yield();
                        break;
                    case DefaultAction::Continue:
                        proc.state = process::State::Runnable;
                        break;
                }
            } else {
                newTF = tf;

                // Create siginfo_t and return address on the userland stack
                // TODO this could fault
                newTF.rsp -= sizeof(siginfo_t);
                auto siginfo = reinterpret_cast<siginfo_t*>(newTF.rsp);
                *siginfo = {};
                siginfo->si_signo = signo;
                newTF.rsp -= 8;
                *reinterpret_cast<uint64_t*>(newTF.rsp) = reinterpret_cast<uint64_t>(action.restorer);

                // Setup arguments (we always add the arguments for siginfo)
                newTF.rdi = signo;
                newTF.rsi = reinterpret_cast<uint64_t>(siginfo);
                newTF.rdx = 0; // TODO: void*
                newTF.rip = reinterpret_cast<uint64_t>(action.handler);

                // Adjust the rsp0 so that we'll keep the original trapframe, which is needed
                // by sigreturn() (which will also undo the adjust)
                proc.rsp0 -= sizeof(amd64::TrapFrame);
                process::UpdateKernelStackForProcess(proc);
                return newTF;
            }
        }
        return tf;
    }
}

extern "C" amd64::TrapFrame* deliver_signal(amd64::TrapFrame* tf, amd64::TrapFrame* newTF)
{
    return &signal::DeliverSignal(*tf, *newTF);
}
