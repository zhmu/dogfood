/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <dogfood/syscall.h>

namespace syscall {
    enum class ArgumentType {
        Int,
        IntPtr,
        FD,
        Void,
        Size,
        PathString,
        SizePtr,
        PID,
        CharPtrArray,
        OffsetPtr,
        CharPtr,
    };

    enum class Direction { In, Out, InOut };

    struct SyscallArgument {
        const char* name = nullptr;
        ArgumentType type{};
        Direction dir{};
    };

    enum class ReturnType { Void, IntOrErrNo };

    struct Syscall {
        int num;
        const char* name;
        ReturnType retType;
        const SyscallArgument args[5];
    };

    constexpr Syscall unknown_syscall{
        0, "???", ReturnType::IntOrErrNo,
        {{"arg1", ArgumentType::Void, Direction::In},
        {"arg2", ArgumentType::Void, Direction::In},
        {"arg3", ArgumentType::Void, Direction::In},
        {"arg4", ArgumentType::Void, Direction::In},
        {"arg5", ArgumentType::Void, Direction::In}}
    };

    // clang-format off
    constexpr Syscall syscalls[] = {
        {
            SYS_exit, "exit", ReturnType::Void,
            {
                {"code", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_read, "read", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"buf", ArgumentType::Void, Direction::In},
                {"size", ArgumentType::Size, Direction::In}
            }
        },
        {
            SYS_write, "write", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"buf", ArgumentType::Void, Direction::Out},
                {"size", ArgumentType::Size, Direction::In}
            }
        },
        {
            SYS_open, "open", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In},
                {"flags", ArgumentType::Int, Direction::In},
                {"mode", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_close, "close", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In}
            }
        },
        {
            SYS_unlink, "unlink", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In}
            }
        },
        {
            SYS_seek, "seek", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"offset", ArgumentType::OffsetPtr, Direction::InOut},
                {"whence", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_clone, "clone", ReturnType::IntOrErrNo,
            {
                {"flags", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_waitpid, "waitpid", ReturnType::IntOrErrNo,
            {
                {"pid", ArgumentType::PID, Direction::In},
                {"stat_loc", ArgumentType::IntPtr, Direction::Out},
                {"options", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_execve, "execve", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In},
                {"argv", ArgumentType::CharPtrArray, Direction::In},
                {"envp", ArgumentType::CharPtrArray, Direction::In}
            }
        },
        {
            SYS_vmop, "vmop", ReturnType::IntOrErrNo,
            {
                {"opts", ArgumentType::Void, Direction::In}
            }
        },
        {
            SYS_dup, "dup", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In}
            }
        },
        {
            SYS_rename, "rename", ReturnType::IntOrErrNo,
            {
                {"oldpath", ArgumentType::PathString, Direction::In},
                {"newpath", ArgumentType::PathString, Direction::In}
            }
        },
        {
            SYS_chdir, "chdir", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In}
            }
        },
        {
            SYS_fstat, "fstat", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In}, {"buf", ArgumentType::Void, Direction::Out}
            }
        },
        {
            SYS_fchdir, "fchdir", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In}
            }
        },
        {
            SYS_fcntl, "fcntl", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In}, {"cmd", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_link, "link", ReturnType::IntOrErrNo,
            {
                {"oldpath", ArgumentType::PathString, Direction::In},
                {"newpath", ArgumentType::PathString, Direction::In}
            }
        },
        {
            SYS_utime, "utime", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In},
                {"times", ArgumentType::Void, Direction::In}
            }
        },
        {
            SYS_clock_settime, "clock_settime", ReturnType::IntOrErrNo,
            {
                {"id", ArgumentType::Int, Direction::In},
                {"tp", ArgumentType::Void, Direction::In}
            }
        },
        {
            SYS_clock_gettime, "clock_gettime", ReturnType::IntOrErrNo,
            {
                {"id", ArgumentType::Int, Direction::In},
                {"tp", ArgumentType::Void, Direction::Out}
            }
        },
        {
            SYS_clock_getres, "clock_getres", ReturnType::IntOrErrNo,
            {
                {"id", ArgumentType::Int, Direction::In},
                {"res", ArgumentType::Void, Direction::Out}
            }
        },
        {
            SYS_readlink, "readlink", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In},
                {"buffer", ArgumentType::Void, Direction::Out},
                {"bufsize", ArgumentType::Size, Direction::In}
            }
        },
        {
            SYS_getcwd, "getcwd", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::Void, Direction::Out},
                {"bufsize", ArgumentType::Size, Direction::In}
            }
        },
        {
            SYS_sigaction, "sigaction", ReturnType::IntOrErrNo,
            {
                {"signum", ArgumentType::Int, Direction::In},
                {"act", ArgumentType::Void, Direction::In},
                {"oldact", ArgumentType::Void, Direction::Out},
            }
        },
        {
            SYS_sigprocmask, "sigprocmask", ReturnType::IntOrErrNo,
            {
                {"how", ArgumentType::Int, Direction::In},
                {"set", ArgumentType::Void, Direction::In},
                {"oldset", ArgumentType::Void, Direction::Out},
            }
        },
        {
            SYS_sigsuspend, "sigsuspend", ReturnType::IntOrErrNo,
            {
                {"mask", ArgumentType::Void, Direction::In},
            }
        },
        {
            SYS_kill, "kill", ReturnType::IntOrErrNo,
            {
                {"pid", ArgumentType::PID, Direction::In},
                {"sig", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_sigreturn, "sigreturn", ReturnType::IntOrErrNo,
        },
        {
            SYS_ioctl, "ioctl", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"op", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_getpgrp, "setpgrp", ReturnType::IntOrErrNo,
        },
        {
            SYS_setpgid, "setpgid", ReturnType::IntOrErrNo,
            {
                {"pid", ArgumentType::PID, Direction::In},
                {"pgid", ArgumentType::PID, Direction::In}
            }
        },
        {
            SYS_setsid, "setsid", ReturnType::IntOrErrNo,
        },
        {
            SYS_dup2, "dup2", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"newindex", ArgumentType::FD, Direction::In}
            }
        },
        {
            SYS_mount, "mount", ReturnType::IntOrErrNo,
            {
                {"type", ArgumentType::CharPtr, Direction::In},
                {"source", ArgumentType::PathString, Direction::In},
                {"dir", ArgumentType::PathString, Direction::In},
                {"flags", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_unmount, "unmount", ReturnType::IntOrErrNo,
            {
                {"dir", ArgumentType::PathString, Direction::In},
                {"flags", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_statfs, "statfs", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In},
                {"buf", ArgumentType::Void, Direction::Out}
            }
        },
        {
            SYS_fstatfs, "fstatfs", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"buf", ArgumentType::Void, Direction::Out}
            }
        },
        {
            SYS_nanosleep, "nanosleep", ReturnType::IntOrErrNo,
            {
                {"rqtp", ArgumentType::Void, Direction::In},
                {"rmtp", ArgumentType::Void, Direction::Out}
            }
        },
        {
            SYS_getsid, "getsid", ReturnType::IntOrErrNo,
            {
                {"pid", ArgumentType::PID, Direction::In}
            }
        },
        {
            SYS_getuid, "getuid", ReturnType::IntOrErrNo,
        },
        {
            SYS_geteuid, "getuid", ReturnType::IntOrErrNo,
        },
        {
            SYS_getgid, "getgid", ReturnType::IntOrErrNo,
        },
        {
            SYS_getegid, "getegid", ReturnType::IntOrErrNo,
        },
        {
            SYS_getpid, "getpid", ReturnType::IntOrErrNo,
        },
        {
            SYS_getppid, "getppid", ReturnType::IntOrErrNo,
        },
        {
            SYS_symlink, "symlink", ReturnType::IntOrErrNo,
            {
                {"oldpath", ArgumentType::PathString, Direction::In},
                {"newpath", ArgumentType::PathString, Direction::In}
            }
        },
        {
            SYS_reboot, "reboot", ReturnType::IntOrErrNo,
            {
                {"how", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_chown, "chown", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In},
                {"uid", ArgumentType::Int, Direction::In},
                {"gid", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_fchown, "fchown", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"uid", ArgumentType::Int, Direction::In},
                {"gid", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_umask, "umask", ReturnType::IntOrErrNo,
            {
                {"mask", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_chmod, "chmod", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In},
                {"mode", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_mkdir, "mkdir", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In}
            }
        },
        {
            SYS_mkdir, "rmdir", ReturnType::IntOrErrNo,
            {
                {"path", ArgumentType::PathString, Direction::In}
            }
        },
        {
            SYS_fchmod, "fchmod", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"mode", ArgumentType::Int, Direction::In}
            }
        },
        {
            SYS_procinfo, "procinfo", ReturnType::IntOrErrNo,
            {
                {"pid", ArgumentType::Int, Direction::In},
                {"size", ArgumentType::Int, Direction::In},
                {"procinfo", ArgumentType::Void, Direction::InOut}
            }
        },
        {
            SYS_fstatat, "fstatat", ReturnType::IntOrErrNo,
            {
                {"fd", ArgumentType::FD, Direction::In},
                {"path", ArgumentType::PathString, Direction::In},
                {"buf", ArgumentType::Void, Direction::Out},
                {"flags", ArgumentType::Void, Direction::In}
            }
        },
        {
            SYS_uname, "uname", ReturnType::IntOrErrNo,
            {
                {"uts", ArgumentType::Void, Direction::Out}
            }
        },
        {
            SYS_ptrace, "ptrace", ReturnType::IntOrErrNo,
            {
                {"req", ArgumentType::Int, Direction::In},
                {"pid", ArgumentType::Int, Direction::In},
                {"addr", ArgumentType::Void, Direction::In},
                {"data", ArgumentType::Int, Direction::In}
            }
        },
    };
    // clang-format on

    const Syscall& LookupSyscall(const user_registers& regs)
    {
        const auto no = regs.rax;
        for (const Syscall& syscall : syscalls) {
            if (syscall.num == no)
                return syscall;
        }
        return unknown_syscall;
    }

}
