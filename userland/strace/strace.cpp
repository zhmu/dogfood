/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <dogfood/user.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <optional>
#include <utility>
#include <cstring>
#include "syscall.h"

using ptrace_request = int;

namespace
{
    static inline constexpr auto DEBUG_PTRACE = false;

    void Debug(const char* fmt, ...)
    {
        if constexpr (DEBUG_PTRACE) {
            va_list va;
            va_start(va, fmt);
            vprintf(fmt, va);
            va_end(va);
        }
    }

    const char* SignalToString(int sig)
    {
#define C(name) case name: return #name
        switch(sig) {
            C(SIGABRT);
            C(SIGALRM);
            C(SIGBUS);
            C(SIGCHLD);
            C(SIGCONT);
            C(SIGFPE);
            C(SIGHUP);
            C(SIGILL);
            C(SIGINT);
            C(SIGIO);
            C(SIGKILL);
            C(SIGPIPE);
            C(SIGPROF);
            C(SIGPWR);
            C(SIGQUIT);
            C(SIGSEGV);
            C(SIGSTOP);
            C(SIGTSTP);
            C(SIGSYS);
            C(SIGTERM);
            C(SIGTRAP);
            C(SIGTTIN);
            C(SIGTTOU);
            C(SIGURG);
            C(SIGUSR1);
            C(SIGUSR2);
            C(SIGVTALRM);
            C(SIGXCPU);
            C(SIGXFSZ);
            C(SIGWINCH);
        }
#undef C
        return "???";
    }

    constexpr const char* errnoStrings[] = {
        "E2BIG",        "EACCES",          "EADDRINUSE",  "EADDRNOTAVAIL", "EAFNOSUPPORT", "EAGAIN",
        "EALREADY",     "EBADF",           "EBADMSG",     "EBUSY",         "ECANCELED",    "ECHILD",
        "ECONNABORTED", "ECONNREFUSED",    "ECONNRESET",  "EDEADLK",       "EDESTADDRREQ", "EDOM",
        "EDQUOT",       "EEXIST",          "EFAULT",      "EFBIG",         "EHOSTUNREACH", "EIDRM",
        "EILSEQ",       "EINPROGRESS",     "EINTR",       "EINVAL",        "EIO",          "EISCONN",
        "EISDIR",       "ELOOP",           "EMFILE",      "EMLINK",        "EMSGSIZE",     "EMULTIHOP",
        "ENAMETOOLONG", "ENETDOWN",        "ENETRESET",   "ENETUNREACH",   "ENFILE",       "ENOBUFS",
        "ENODATA",      "ENODEV",          "ENOENT",      "ENOEXEC",       "ENOLCK",       "ENOLINK",
        "ENOMEM",       "ENOMSG",          "ENOPROTOOPT", "ENOSPC",        "ENOSR",        "ENOSTR",
        "ENOSYS",       "ENOTCONN",        "ENOTDIR",     "ENOTEMPTY",     "ENOTSOCK",     "ENOTSUP",
        "ENOTTY",       "ENXIO",           "EOPNOTSUPP",  "EOVERFLOW",     "EPERM",        "EPIPE",
        "EPROTO",       "EPROTONOSUPPORT", "EPROTOTYPE",  "ERANGE",        "EROFS",        "ESPIPE",
        "ESRCH",        "ESTALE",          "ETIME",       "ETIMEDOUT",     "ETXTBSY",      "EXDEV"};

    const char* ErrnoToString(int n)
    {
        if (n < 1 || n > (sizeof(errnoStrings) / sizeof(errnoStrings[0]) - 1))
            return "?";
        return errnoStrings[n - 1];
    }

    template<typename T> struct always_false : std::false_type { };

    template<typename T>
    T GetArgument(const user_registers& regs, const size_t n)
    {
        const uint64_t value = [&]() {
            switch(n) {
                case 1: return regs.rdi;
                case 2: return regs.rsi;
                case 3: return regs.rdx;
                case 4: return regs.r10;
                case 5: return regs.r8;
                case 6: return regs.r9;
                default: std::unreachable();
            }
        }();
        return reinterpret_cast<T>(value);
    }

    void PrintArguments(const user_registers& regs, const syscall::SyscallArgument args[])
    {
        int n = 1, m = 0;
        for (const auto* arg = &args[0]; arg->name != nullptr; ++arg, ++n) {
            if (m++ > 0)
                printf(", ");
            switch (arg->type) {
                case syscall::ArgumentType::Int:
                case syscall::ArgumentType::FD:
                case syscall::ArgumentType::Size:
                case syscall::ArgumentType::PID: {
                    const auto p = GetArgument<unsigned long>(regs, n);
                    printf("%s: %d", arg->name, p);
                    break;
                }
                case syscall::ArgumentType::PathString:
                case syscall::ArgumentType::CharPtr: {
                    const auto p = GetArgument<const void*>(regs, n);
                    printf("%s: %p", arg->name, p);
                    break;
                }
                case syscall::ArgumentType::OffsetPtr:
                case syscall::ArgumentType::SizePtr: {
                    const auto p = GetArgument<const long*>(regs, n);
                    printf("%s: %p", arg->name, p);
                    break;
                }
                case syscall::ArgumentType::IntPtr:
                case syscall::ArgumentType::Void:
                case syscall::ArgumentType::CharPtrArray:
                default: {
                    const auto p = GetArgument<const void*>(regs, n);
                    printf("%s: %p", arg->name, p);
                    break;
                }
            }
        }
    }

    void ShowErrorAndTerminate(const char* msg)
    {
        perror(msg);
        exit(EXIT_FAILURE);
    }

    void InvokePTrace(ptrace_request request, pid_t pid, void* addr, unsigned long data)
    {
        if (ptrace(request, pid, addr, reinterpret_cast<void*>(data)) < 0)
            ShowErrorAndTerminate("ptrace");
    }

    void PrintRegisters(const user_registers& regs)
    {
        printf("registers rax %08x rbx %08x rcx %08x rdx %08x\n",
         regs.rax, regs.rbx, regs.rcx, regs.rdx);
        printf("registers rbp %08x rsi %08x rdi %08x r8  %08x\n",
         regs.rbp, regs.rsi, regs.rdi, regs.r8);
        printf("registers r9  %08x r10 %08x r11 %08x r12 %08x\n",
         regs.r9, regs.r10, regs.r11, regs.r12);
        printf("registers r13 %08x r14 %08x r15 %08x rip %08x\n",
         regs.r13, regs.r14, regs.r15, regs.rip);
        printf("registers rfl %08x rsp %08x cs  %08x ds  %08x\n",
         regs.rflags, regs.rsp, regs.cs, regs.ds);
        printf("registers es  %08x fs  %08x gs  %08x ss  %08x\n",
         regs.es, regs.fs, regs.gs, regs.ss);
    }

    [[noreturn]] void Child(int tracee_index, int argc, char* argv[])
    {
        auto tracee_argv = new char*[(argc - tracee_index) + 1];
        int n = 0;
        for(int i = tracee_index; i < argc; ++i, ++n)
            tracee_argv[n] = argv[i];
        tracee_argv[n] = nullptr;

        InvokePTrace(PTRACE_TRACEME, 0, 0, 0);
        execv(argv[tracee_index], tracee_argv);
        exit(EXIT_FAILURE);
    }

    std::optional<int> DetermineTracee(int argc, char* argv[])
    {
        // Look at the last argument
        int n = 1;
        while(n < argc && argv[n][0] == '-') {
            if (strcmp(argv[n], "--") == 0) break;
            ++n;
        }

        if (n == argc)
            return {};
        return n;
    }

    [[noreturn]] void usage(const char* progname)
    {
        printf("usage: %s tracee arguments ...\n", progname);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[])
{
    auto tracee_index = DetermineTracee(argc, argv);
    if (!tracee_index) usage(argv[0]);

    int pid = fork();
    if (pid == 0) {
        Child(*tracee_index, argc, argv);
    }
    Debug("tracer: my pid %d child %d\n", getpid(), pid);

    const syscall::Syscall* current_syscall = nullptr;
    int status;
    do {
        const auto w = waitpid(pid, &status, WUNTRACED /* | WCONTINUED */);
        if (w < 0)
            ShowErrorAndTerminate("waitpid");

        if (WIFEXITED(status)) {
            printf(">> exited, status %d\n",
                WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf(">> killed by signal %s (%d)\n",
                SignalToString(WTERMSIG(status)), WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            Debug("stopped by signal %s (%d)\n",
                SignalToString(WSTOPSIG(status)), WSTOPSIG(status));
            if (WSTOPSIG(status) == SIGTRAP) {
                struct user_registers regs;
                InvokePTrace(PTRACE_GETREGS, pid, 0, reinterpret_cast<unsigned long>(&regs));

                if (!current_syscall) {
                    current_syscall = &syscall::LookupSyscall(regs);
                    if (current_syscall == &syscall::unknown_syscall)
                        printf("unknown_%d", regs.rax);
                    else
                        printf("%s", current_syscall->name);
                    printf("(");
                    PrintArguments(regs, current_syscall->args);
                    printf(")");
                    if (current_syscall->retType == syscall::ReturnType::Void)
                        printf("\n");
                    fflush(stdout);
                } else {
                    switch(current_syscall->retType) {
                        case syscall::ReturnType::Void:
                            break;
                        case syscall::ReturnType::IntOrErrNo: {
                            const auto value = static_cast<int64_t>(regs.rax);
                            printf(" = ");
                            if (value < 0) {
                                printf("%s", ErrnoToString(-value));
                            } else {
                                printf("%lu", value);
                            }
                            break;
                        }
                    }
                    printf("\n");
                    current_syscall = nullptr;
                    fflush(stdout);
                }

            }

            InvokePTrace(PTRACE_SYSCALL, pid, NULL, 0);
#if 0
        } else if (WIFCONTINUED(status)) {
#endif
        }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
}
