#include "types.h"
#include "syscall.h"
#include "exec.h"
#include "file.h"
#include "ext2.h"
#include "process.h"
#include "lib.h"
#include "vm.h"
#include <dogfood/errno.h>
#include <dogfood/fcntl.h>
#include <dogfood/stat.h>
#include <dogfood/syscall.h>

#include "x86_64/amd64.h"
#include "hw/console.h"

#define DEBUG_SYSCALL 0

#if DEBUG_SYSCALL
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

struct Syscall {
    const char* name;
    int num;
    const SyscallArgument args[5];
};

constexpr Syscall syscalls[] = {
    {"exit", SYS_exit, {{"code", ArgumentType::Int, Direction::In}}},
    {"read",
     SYS_read,
     {{"fd", ArgumentType::FD, Direction::In},
      {"buf", ArgumentType::Void, Direction::In},
      {"size", ArgumentType::Size, Direction::In}}},
    {"write",
     SYS_write,
     {{"fd", ArgumentType::FD, Direction::In},
      {"buf", ArgumentType::Void, Direction::Out},
      {"size", ArgumentType::Size, Direction::In}}},
    {"open ",
     SYS_open,
     {{"path", ArgumentType::PathString, Direction::In},
      {"flags", ArgumentType::Int, Direction::In},
      {"mode", ArgumentType::Int, Direction::In}}},
    {"close", SYS_close, {{"fd", ArgumentType::FD, Direction::In}}},
    {"unlink", SYS_unlink, {{"path", ArgumentType::PathString, Direction::In}}},
    {"seek",
     SYS_seek,
     {{"fd", ArgumentType::FD, Direction::In},
      {"offset", ArgumentType::OffsetPtr, Direction::InOut},
      {"whence", ArgumentType::Int, Direction::In}}},
    {"clone", SYS_clone, {{"flags", ArgumentType::Int, Direction::In}}},
    {"waitpid",
     SYS_waitpid,
     {{"pid", ArgumentType::PID, Direction::In},
      {"stat_loc", ArgumentType::IntPtr, Direction::Out},
      {"options", ArgumentType::Int, Direction::In}}},
    {"execve",
     SYS_execve,
     {{"path", ArgumentType::PathString, Direction::In},
      {"argv", ArgumentType::CharPtrArray, Direction::In},
      {"envp", ArgumentType::CharPtrArray, Direction::In}}},
    {"vmop", SYS_vmop, {{"opts", ArgumentType::Void, Direction::In}}},
    {"dup", SYS_dup, {{"fd", ArgumentType::FD, Direction::In}}},
    {"rename",
     SYS_rename,
     {{"oldpath", ArgumentType::PathString, Direction::In},
      {"newpath", ArgumentType::PathString, Direction::In}}},
    {"stat",
     SYS_stat,
     {{"path", ArgumentType::PathString, Direction::In},
      {"buf", ArgumentType::Void, Direction::Out}}},
    {"chdir", SYS_chdir, {{"path", ArgumentType::PathString, Direction::In}}},
    {"fstat",
     SYS_fstat,
     {{"fd", ArgumentType::FD, Direction::In}, {"buf", ArgumentType::Void, Direction::Out}}},
    {"fchdir", SYS_fchdir, {{"fd", ArgumentType::FD, Direction::In}}},
    {"fcntl",
     SYS_fcntl,
     {{"fd", ArgumentType::FD, Direction::In}, {"cmd", ArgumentType::Int, Direction::In}}},
    {"link",
     SYS_link,
     {{"oldpath", ArgumentType::PathString, Direction::In},
      {"newpath", ArgumentType::PathString, Direction::In}}},
    {"utime",
     SYS_utime,
     {{"path", ArgumentType::PathString, Direction::In},
      {"times", ArgumentType::Void, Direction::In}}},
    {"clock_settime",
     SYS_clock_settime,
     {{"id", ArgumentType::Int, Direction::In}, {"tp", ArgumentType::Void, Direction::In}}},
    {"clock_gettime",
     SYS_clock_gettime,
     {{"id", ArgumentType::Int, Direction::In}, {"tp", ArgumentType::Void, Direction::Out}}},
    {"clock_getres",
     SYS_clock_getres,
     {{"id", ArgumentType::Int, Direction::In}, {"res", ArgumentType::Void, Direction::Out}}},
    {"readlink",
     SYS_readlink,
     {{"path", ArgumentType::PathString, Direction::In},
      {"buffer", ArgumentType::Void, Direction::Out},
      {"bufsize", ArgumentType::Size, Direction::In}}},
    {"lstat",
     SYS_lstat,
     {{"path", ArgumentType::PathString, Direction::In},
      {"buf", ArgumentType::Void, Direction::Out}}},
    {"getcwd",
     SYS_getcwd,
     {{"path", ArgumentType::Void, Direction::Out},
      {"bufsize", ArgumentType::Size, Direction::In}}},
    {"sigaction", SYS_sigaction},
    {"sigprocmask", SYS_sigprocmask},
    {"sigsuspend", SYS_sigsuspend},
    {"kill",
     SYS_kill,
     {{"pid", ArgumentType::PID, Direction::In}, {"sig", ArgumentType::Int, Direction::In}}},
    {"sigreturn", SYS_sigreturn},
    {"ioctl",
     SYS_ioctl,
     {{"fd", ArgumentType::FD, Direction::In}, {"op", ArgumentType::Int, Direction::In}}},
    {"setpgrp", SYS_getpgrp},
    {"setpgid",
     SYS_setpgid,
     {{"pid", ArgumentType::PID, Direction::In}, {"pgid", ArgumentType::PID, Direction::In}}},
    {"setsid", SYS_setsid},
    {"dup2",
     SYS_dup2,
     {{"fd", ArgumentType::FD, Direction::In}, {"newindex", ArgumentType::FD, Direction::In}}},
    {"mount",
     SYS_mount,
     {{"type", ArgumentType::CharPtr, Direction::In},
      {"source", ArgumentType::PathString, Direction::In},
      {"dir", ArgumentType::PathString, Direction::In},
      {"flags", ArgumentType::Int, Direction::In}}},
    {"unmount",
     SYS_unmount,
     {{"dir", ArgumentType::PathString, Direction::In},
      {"flags", ArgumentType::Int, Direction::In}}},
    {"statfs",
     SYS_statfs,
     {{"path", ArgumentType::PathString, Direction::In},
      {"buf", ArgumentType::Void, Direction::Out}}},
    {"fstatfs",
     SYS_fstatfs,
     {{"fd", ArgumentType::FD, Direction::In}, {"buf", ArgumentType::Void, Direction::Out}}},
    {"nanosleep",
     SYS_nanosleep,
     {{"rqtp", ArgumentType::Void, Direction::In}, {"rmtp", ArgumentType::Void, Direction::Out}}},
    {"getsid", SYS_getsid, {{"pid", ArgumentType::PID, Direction::In}}},
    {"getuid", SYS_getuid},
    {"getuid", SYS_geteuid},
    {"getgid", SYS_getgid},
    {"getegid", SYS_getegid},
    {"getpid", SYS_getpid},
    {"getppid", SYS_getppid},
    {"symlink",
     SYS_symlink,
     {{"oldpath", ArgumentType::PathString, Direction::In},
      {"newpath", ArgumentType::PathString, Direction::In}}},
    {"reboot", SYS_reboot, {{"how", ArgumentType::Int, Direction::In}}},
    {"chown",
     SYS_chown,
     {{"path", ArgumentType::PathString, Direction::In},
      {"uid", ArgumentType::Int, Direction::In},
      {"gid", ArgumentType::Int, Direction::In}}},
    {"fchown",
     SYS_fchown,
     {{"fd", ArgumentType::FD, Direction::In},
      {"uid", ArgumentType::Int, Direction::In},
      {"gid", ArgumentType::Int, Direction::In}}},
    {"umask", SYS_umask, {{"mask", ArgumentType::Int, Direction::In}}},
    {"chmod",
     SYS_chmod,
     {{"path", ArgumentType::PathString, Direction::In},
      {"mode", ArgumentType::Int, Direction::In}}},
    {"mkdir", SYS_mkdir, {{"path", ArgumentType::PathString, Direction::In}}},
    {"rmdir", SYS_mkdir, {{"path", ArgumentType::PathString, Direction::In}}},
    {"fchmod",
     SYS_fchmod,
     {{"fd", ArgumentType::FD, Direction::In}, {"mode", ArgumentType::Int, Direction::In}}},
};

const char* errnoStrings[] = {
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

const char* ErrnoToString(int errno)
{
    if (errno < 1 || errno > (sizeof(errnoStrings) / sizeof(errnoStrings[0]) - 1))
        return "?";
    return errnoStrings[errno - 1];
}

const Syscall* GetSyscallByNumber(int no)
{
    for (const Syscall& syscall : syscalls) {
        if (syscall.num == no)
            return &syscall;
    }
    return nullptr;
}

void PrintArguments(amd64::TrapFrame* tf, bool in, const SyscallArgument args[])
{
    auto getArgument = [tf](int n) {
        switch (n) {
            case 1:
                return syscall::GetArgument<1>(*tf);
            case 2:
                return syscall::GetArgument<2>(*tf);
            case 3:
                return syscall::GetArgument<3>(*tf);
            case 4:
                return syscall::GetArgument<4>(*tf);
            case 5:
                return syscall::GetArgument<5>(*tf);
            default:
                return static_cast<unsigned long>(0);
        }
    };

    int n = 1, m = 0;
    for (const auto* arg = &args[0]; arg->name != nullptr; ++arg, ++n) {
        if (in) {
            if (arg->dir == Direction::Out)
                continue;
        } else {
            if (arg->dir == Direction::In)
                continue;
        }
        if (m++ > 0)
            printf(", ");
        switch (arg->type) {
            case ArgumentType::Int:
            case ArgumentType::FD:
            case ArgumentType::Size:
            case ArgumentType::PID: {
                auto p = reinterpret_cast<unsigned long>(getArgument(n));
                printf("%s: %ld", arg->name, p);
                break;
            }
            case ArgumentType::PathString:
            case ArgumentType::CharPtr: {
                auto p = reinterpret_cast<const void*>(getArgument(n));
                printf("%s: '%s'", arg->name, p);
                break;
            }
            case ArgumentType::OffsetPtr:
            case ArgumentType::SizePtr: {
                auto p = reinterpret_cast<const long*>(getArgument(n));
                printf("*%s: %ld", arg->name, *p);
                break;
            }
            case ArgumentType::IntPtr:
            case ArgumentType::Void:
            case ArgumentType::CharPtrArray:
            default: {
                auto p = reinterpret_cast<const void*>(getArgument(n));
                printf("%s: %p", arg->name, p);
                break;
            }
        }
    }
}
#endif

namespace
{
    int DupFD(file::File& file)
    {
        auto& current = process::GetCurrent();
        auto file2 = file::Allocate(current);
        if (file2 == nullptr)
            return -ENFILE;
        *file2 = file;
        if (file2->f_inode != nullptr)
            fs::iref(*file2->f_inode);
        return file2 - &current.files[0];
    }

    uint64_t DoSyscall(amd64::TrapFrame* tf)
    {
        const auto num = syscall::GetNumber(*tf);
        switch (num) {
            case SYS_exit:
                return process::Exit(*tf);
            case SYS_write: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                auto buf = reinterpret_cast<const void*>(syscall::GetArgument<2>(*tf));
                auto len = syscall::GetArgument<3>(*tf);
                return file::Write(*file, buf, len);
            }
            case SYS_read: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                auto buf = reinterpret_cast<void*>(syscall::GetArgument<2>(*tf));
                auto len = syscall::GetArgument<3>(*tf);
                return file::Read(*file, buf, len);
            }
            case SYS_open: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto flags = static_cast<int>(syscall::GetArgument<2>(*tf));
                auto mode = static_cast<int>(syscall::GetArgument<3>(*tf));

                auto& current = process::GetCurrent();
                auto file = file::Allocate(current);
                if (file == nullptr)
                    return -ENFILE;

                fs::Inode* inode;
                if (int errno = fs::Open(path, flags, mode & 0777, inode); errno != 0) {
                    file::Free(*file);
                    return -errno;
                }

                file->f_inode = inode;
                return file - &current.files[0];
            }
            case SYS_close: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                file::Free(*file);
                return 0;
            }
            case SYS_stat:
            case SYS_lstat: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto buf = reinterpret_cast<stat*>(syscall::GetArgument<2>(*tf));

                auto inode = fs::namei(path, num != SYS_lstat);
                if (inode == nullptr)
                    return -ENOENT;
                auto ret = fs::Stat(*inode, *buf);
                fs::iput(*inode);
                return ret ? 0 : -EIO;
            }
            case SYS_fstat: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                auto buf = reinterpret_cast<stat*>(syscall::GetArgument<2>(*tf));
                if (file == nullptr)
                    return -EBADF;
                if (file->f_inode == nullptr) {
                    // Assume this is the console
                    memset(buf, 0, sizeof *buf);
                    buf->st_mode = EXT2_S_IFCHR | 0666;
                    return 0;
                }
                return fs::Stat(*file->f_inode, *buf) ? 0 : -EIO;
            }
            case SYS_seek: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                auto offset = reinterpret_cast<long*>(syscall::GetArgument<2>(*tf));
                auto whence = syscall::GetArgument<3>(*tf);
                if (file == nullptr)
                    return -EBADF;
                if (file->f_inode == nullptr || file->f_inode->ext2inode == nullptr)
                    return -ESPIPE;
                long new_offset = file->f_offset;
                auto file_size = file->f_inode->ext2inode->i_size;
                switch (whence) {
                    case SEEK_SET:
                        new_offset = *offset;
                        break;
                    case SEEK_CUR:
                        new_offset += *offset;
                        break;
                    case SEEK_END:
                        new_offset = file->f_inode->ext2inode->i_size - *offset;
                        break;
                }
                if (new_offset < 0)
                    new_offset = 0;
                // Do not limit offset here; writing past the end of the file should be okay
                // TODO check this and make it proper
                /*if (new_offset > file_size)
                    new_offset = file_size;*/
                file->f_offset = new_offset;
                *offset = new_offset;
                return 0;
            }
            case SYS_dup: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                return DupFD(*file);
            }
            case SYS_dup2: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                auto newfd = syscall::GetArgument<2>(*tf);
                auto& current = process::GetCurrent();
                auto file2 = file::AllocateByIndex(current, newfd);
                if (file2 == nullptr)
                    return -ENFILE;
                *file2 = *file;
                if (file2->f_inode != nullptr)
                    fs::iref(*file2->f_inode);
                return file2 - &current.files[0];
            }
            case SYS_fcntl: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                auto op = syscall::GetArgument<2>(*tf);
                switch (op) {
                    case F_DUPFD:
                        return DupFD(*file);
                    case F_GETFD:
                    case F_GETFL:
                    case F_SETFL:
                        return 0;
                    default:
                        printf("fcntl(): op %d not supported\n", op);
                        return -EINVAL;
                }
                return 0;
            }
            case SYS_getcwd: {
                auto buf = reinterpret_cast<char*>(syscall::GetArgument<1>(*tf));
                auto len = syscall::GetArgument<2>(*tf);
                auto& current = process::GetCurrent();
                // TODO do this in userspace instead
                return -fs::ResolveDirectoryName(*current.cwd, buf, len);
            }
            case SYS_chdir: {
                auto buf = reinterpret_cast<char*>(syscall::GetArgument<1>(*tf));
                auto& current = process::GetCurrent();
                auto inode = fs::namei(buf, true);
                if (inode == nullptr)
                    return -ENOENT;
                if ((inode->ext2inode->i_mode & EXT2_S_IFDIR) == 0) {
                    fs::iput(*inode);
                    return -ENOTDIR;
                }

                fs::iput(*current.cwd);
                current.cwd = inode;
                return 0;
            }
            case SYS_fchdir: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                if ((file->f_inode->ext2inode->i_mode & EXT2_S_IFDIR) == 0)
                    return -ENOTDIR;

                auto& current = process::GetCurrent();
                fs::iput(*current.cwd);
                current.cwd = file->f_inode;
                fs::iref(*current.cwd);
                return 0;
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
                return process::GetCurrent().pid;
            case SYS_getppid:
                return process::GetCurrent().ppid;
            case SYS_sigaction:
                return 0; // not implemented
            case SYS_clock_gettime:
                return -ENOSYS;
            case SYS_chown: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto uid = static_cast<int>(syscall::GetArgument<2>(*tf));
                auto gid = static_cast<int>(syscall::GetArgument<3>(*tf));
                auto inode = fs::namei(path, true);
                if (inode == nullptr)
                    return -ENOENT;

                inode->ext2inode->i_uid = uid;
                inode->ext2inode->i_gid = gid;
                fs::idirty(*inode);
                fs::iput(*inode);
                return 0;
            }
            case SYS_umask:
                break;
            case SYS_chmod: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto mode = static_cast<int>(syscall::GetArgument<2>(*tf));
                auto inode = fs::namei(path, true);
                if (inode == nullptr)
                    return -ENOENT;
                mode &= 0777;
                inode->ext2inode->i_mode = (inode->ext2inode->i_mode & ~0777) | mode;
                fs::idirty(*inode);
                fs::iput(*inode);
                return 0;
            }
            case SYS_unlink: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                return -fs::Unlink(path);
            }
            case SYS_mkdir: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto mode = static_cast<int>(syscall::GetArgument<2>(*tf));
                return -fs::MakeDirectory(path, mode & 0777);
            }
            case SYS_rmdir: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                return -fs::RemoveDirectory(path);
            }
            case SYS_fchown: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                if (file->f_inode == nullptr || file->f_inode->ext2inode == nullptr)
                    return -ENOENT;
                auto uid = static_cast<int>(syscall::GetArgument<2>(*tf));
                auto gid = static_cast<int>(syscall::GetArgument<3>(*tf));
                file->f_inode->ext2inode->i_uid = uid;
                file->f_inode->ext2inode->i_gid = gid;
                fs::idirty(*file->f_inode);
                return 0;
            }
            case SYS_fchmod: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return -EBADF;
                if (file->f_inode == nullptr || file->f_inode->ext2inode == nullptr)
                    return -ENOENT;
                auto mode = static_cast<int>(syscall::GetArgument<2>(*tf));
                mode &= 0777;
                file->f_inode->ext2inode->i_mode =
                    (file->f_inode->ext2inode->i_mode & ~0777) | mode;
                fs::idirty(*file->f_inode);
                fs::iput(*file->f_inode);
                return 0;
            }
            case SYS_link: {
                auto oldPath = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto newPath = reinterpret_cast<const char*>(syscall::GetArgument<2>(*tf));
                return -fs::Link(oldPath, newPath);
            }
            case SYS_readlink: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto buf = reinterpret_cast<char*>(syscall::GetArgument<2>(*tf));
                auto size = reinterpret_cast<size_t>(syscall::GetArgument<2>(*tf));
                auto inode = fs::namei(path, false);
                if (inode == nullptr)
                    return -ENOENT;
                if ((inode->ext2inode->i_mode & EXT2_S_IFMASK) != EXT2_S_IFLNK) {
                    fs::iput(*inode);
                    return -EINVAL;
                }
                const auto numBytesRead = fs::Read(*inode, buf, 0, size);
                fs::iput(*inode);
                return numBytesRead;
            }
            case SYS_symlink: {
                auto oldPath = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto newPath = reinterpret_cast<const char*>(syscall::GetArgument<2>(*tf));
                return -fs::SymLink(oldPath, newPath);
            }
        }
        printf(
            "[%d] unsupported syscall %d %lx [%x %x %x %x %x %x]\n", process::GetCurrent().pid,
            syscall::GetNumber(*tf), syscall::GetArgument<1>(*tf), syscall::GetArgument<2>(*tf),
            syscall::GetArgument<3>(*tf), syscall::GetArgument<4>(*tf),
            syscall::GetArgument<5>(*tf), syscall::GetArgument<6>(*tf));
        return -1;
    }
} // namespace

extern "C" uint64_t perform_syscall(amd64::TrapFrame* tf)
{
#if DEBUG_SYSCALL
    bool quiet = syscall::GetNumber(*tf) == SYS_read || syscall::GetNumber(*tf) == SYS_write;

    const Syscall* syscall = GetSyscallByNumber(syscall::GetNumber(*tf));
    if (!quiet) {
        if (syscall == nullptr) {
            printf(
                "[%d] ??? (%d) %lx [%x %x %x %x %x %x] ->", process::GetCurrent().pid,
                syscall::GetNumber(*tf), syscall::GetArgument<1>(*tf), syscall::GetArgument<2>(*tf),
                syscall::GetArgument<3>(*tf), syscall::GetArgument<4>(*tf),
                syscall::GetArgument<5>(*tf), syscall::GetArgument<6>(*tf));
        } else {
            printf("[%d] %s (", process::GetCurrent().pid, syscall->name);
            PrintArguments(tf, true, syscall->args);
            printf(") ->");
        }
    }
#endif
    const auto result = DoSyscall(tf);
#if DEBUG_SYSCALL
    if (!quiet) {
        if (auto r = static_cast<int64_t>(result); r < 0)
            printf(" -%s", ErrnoToString(-r));
        else
            printf(" %ld", result);
        if (syscall != nullptr) {
            printf(" (");
            PrintArguments(tf, false, syscall->args);
            printf(")");
        }
        printf("\n");
    }
#endif
    return result;
}
