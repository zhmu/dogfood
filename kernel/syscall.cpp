#include "types.h"

#include "syscall.h"
#include "exec.h"
#include "file.h"
#include "error.h"
#include "ext2.h"
#include "pipe.h"
#include "process.h"
#include "ptrace.h"
#include "select.h"
#include "signal.h"
#include "lib.h"
#include "vm.h"
#include <dogfood/fcntl.h>
#include <dogfood/stat.h>
#include <dogfood/syscall.h>
#include <dogfood/utsname.h>

namespace {
    constexpr inline auto modeMask = 0777;
}

namespace
{
    result::MaybeInt DupFD(file::File& file)
    {
        auto& current = process::GetCurrent();
        auto file2 = file::Allocate(current);
        if (file2 == nullptr)
            return result::Error(error::Code::NoFile);
        file::Dup(file, *file2);
        return file2 - &current.files[0];
    }

    template<typename T>
    uint64_t MapError(const result::Maybe<T>& result)
    {
        assert(!result.has_value());
        return -static_cast<int>(result.error());
    }

    uint64_t MapResult(const result::MaybeInt result)
    {
        return result ? *result : MapError(result);
    }

    result::MaybeInt DoSyscall(amd64::TrapFrame* tf)
    {
        const auto num = syscall::GetNumber(*tf);
        switch (num) {
            case SYS_exit:
                return process::Exit(*tf);
            case SYS_write: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                const auto buf = syscall::GetArgument<2, const char*>(*tf);
                const auto len = syscall::GetArgument<3>(*tf);
                return file::Write(*file, buf.get(), len);
            }
            case SYS_read: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                auto buf = syscall::GetArgument<2, char*>(*tf);
                const auto len = syscall::GetArgument<3>(*tf);
                return file::Read(*file, buf.get(), len);
            }
            case SYS_open: {
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                const auto flags = syscall::GetArgument<2, int>(*tf);
                const auto mode = syscall::GetArgument<3, int>(*tf);
                if (flags & (O_RDONLY | O_WRONLY | O_RDWR) == 0)
                    return result::Error(error::Code::InvalidArgument);

                auto& current = process::GetCurrent();
                const auto mask = (~current.umask) & modeMask;

                auto result = fs::Open(path.get(), flags, mode & mask);
                if (!result) {
                    return MapError(result);
                }
                return file::Open(current, std::move(*result), flags);
            }
            case SYS_close: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                file::Free(*file);
                return 0;
            }
            case SYS_fstat: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                auto statBuf = syscall::GetArgument<2, stat*>(*tf);
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);

                stat st{};
                if (file->f_inode == nullptr) {
                    // Assume this is the console
                    st.st_mode = EXT2_S_IFCHR | 0666;
                } else {
                    if (!fs::Stat(*file->f_inode, st))
                        return result::Error(error::Code::IOError);
                }
                return statBuf.Set(st);
            }
            case SYS_fstatat: {
                const auto path = syscall::GetArgument<2, const char*>(*tf);
                auto statBuf = syscall::GetArgument<3, stat*>(*tf);
                const auto flags = syscall::GetArgument<4>(*tf);

                fs::InodeRef base_inode;
                const auto fd = syscall::GetArgument<1>(*tf);
                if (fd == AT_FDCWD) {
                    base_inode = fs::ReferenceInode(process::GetCurrent().cwd);
                } else {
                    const auto file = file::FindByIndex(process::GetCurrent(), fd);
                    if (file == nullptr) return result::Error(error::Code::BadFileHandle);
                    base_inode = fs::ReferenceInode(file->f_inode);
                }

                const auto follow = (flags & AT_SYMLINK_NOFOLLOW) ? fs::Follow::No : fs::Follow::Yes;
                auto inode = fs::namei(path.get(), follow, std::move(base_inode));
                if (!inode) return result::Error(inode.error());

                result::MaybeInt ret = 0;
                stat st{};
                if (fs::Stat(**inode, st)) {
                    if (!statBuf.Set(st)) ret = result::Error(error::Code::MemoryFault);
                } else {
                    ret = result::Error(error::Code::IOError);
                }
                return ret;
            }
            case SYS_seek: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                auto offsetPtr = syscall::GetArgument<2, long*>(*tf);
                const auto whence = syscall::GetArgument<3>(*tf);
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                if (file->f_inode == nullptr || file->f_inode->ext2inode == nullptr)
                    return result::Error(error::Code::InvalidSeek);
                auto offsetArg = *offsetPtr;
                if (!offsetArg)
                    return result::Error(error::Code::MemoryFault);

                long new_offset = file->f_offset;
                auto file_size = file->f_inode->ext2inode->i_size;
                switch (whence) {
                    case SEEK_SET:
                        new_offset = *offsetArg;
                        break;
                    case SEEK_CUR:
                        new_offset += *offsetArg;
                        break;
                    case SEEK_END:
                        new_offset = file->f_inode->ext2inode->i_size - *offsetArg;
                        break;
                }
                if (new_offset < 0)
                    new_offset = 0;
                if (!offsetPtr.Set(new_offset)) return result::Error(error::Code::MemoryFault);
                // Do not limit offset here; writing past the end of the file should be okay
                // TODO check this and make it proper
                /*if (new_offset > file_size)
                    new_offset = file_size;*/
                file->f_offset = new_offset;
                return 0;
            }
            case SYS_dup: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                return DupFD(*file);
            }
            case SYS_dup2: {
                const auto sourceFd = syscall::GetArgument<1>(*tf);
                const auto file = file::FindByIndex(process::GetCurrent(), sourceFd);
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                const auto newFd = syscall::GetArgument<2>(*tf);
                if (sourceFd == newFd) return newFd;
                auto& current = process::GetCurrent();
                auto file2 = file::AllocateByIndex(current, newFd);
                if (file2 == nullptr)
                    return result::Error(error::Code::NoFile);
                file::Dup(*file, *file2);
                return file2 - &current.files[0];
            }
            case SYS_fcntl: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                const auto op = syscall::GetArgument<2>(*tf);
                const auto arg = syscall::GetArgument<3>(*tf);
                switch (op) {
                    case F_DUPFD:
                        return DupFD(*file);
                    case F_GETFD: {
                        int flags = 0;
                        if (file->f_flags & O_CLOEXEC)
                            flags |= FD_CLOEXEC;
                        return flags;
                    }
                    case F_SETFD:
                        if (arg & FD_CLOEXEC) {
                            file->f_flags |= O_CLOEXEC;
                        } else {
                            file->f_flags &= ~O_CLOEXEC;
                        }
                        return 0;
                    case F_GETFL:
                        return file->f_flags;
                    case F_SETFL:
                        if (arg != O_NONBLOCK)
                            return result::Error(error::Code::InvalidArgument);
                        file->f_flags |= O_NONBLOCK;
                        return 0;
                    default:
                        Print("fcntl(): op ", op, " not supported\n");
                        return result::Error(error::Code::InvalidArgument);
                }
                return 0;
            }
            case SYS_getcwd: {
                auto buf = syscall::GetArgument<1, char*>(*tf);
                const auto len = syscall::GetArgument<2>(*tf);
                auto& current = process::GetCurrent();
                return fs::ResolveDirectoryName(*current.cwd, buf.get(), len);
            }
            case SYS_chdir: {
                const auto buf = syscall::GetArgument<1, char*>(*tf);
                auto& current = process::GetCurrent();
                auto inode = fs::namei(buf.get(), fs::Follow::Yes, {});
                if (!inode) return result::Error(inode.error());
                if (((*inode)->ext2inode->i_mode & EXT2_S_IFDIR) == 0) {
                    return result::Error(error::Code::NotADirectory);
                }

                current.cwd = std::move(*inode);
                return 0;
            }
            case SYS_fchdir: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                if ((file->f_inode->ext2inode->i_mode & EXT2_S_IFDIR) == 0)
                    return result::Error(error::Code::NotADirectory);

                auto& current = process::GetCurrent();
                current.cwd = fs::ReferenceInode(file->f_inode);
                return 0;
            }
            case SYS_vmop:
                return vm::VmOp(*tf);
            case SYS_kill:
                return signal::kill(*tf);
            case SYS_clone:
                return process::Fork(*tf);
            case SYS_waitpid:
                return process::WaitPID(*tf);
            case SYS_execve:
                return exec::Exec(*tf);
            case SYS_getsid:
            case SYS_getuid:
            case SYS_geteuid:
            case SYS_getgid:
            case SYS_getegid:
                return 0; // not implemented
            case SYS_getpid:
                return process::GetCurrent().pid;
            case SYS_getppid:
                return process::GetCurrent().parent->pid;
            case SYS_sigaction:
                return signal::sigaction(*tf);
            case SYS_sigreturn:
                return signal::sigreturn(*tf);
            case SYS_clock_gettime:
                return result::Error(error::Code::BadSystemCall);
            case SYS_chown: {
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                const auto uid = syscall::GetArgument<2, int>(*tf);
                const auto gid = syscall::GetArgument<3, int>(*tf);
                const auto inode = fs::namei(path.get(), fs::Follow::Yes, {});
                if (!inode) return result::Error(inode.error());

                (*inode)->ext2inode->i_uid = uid;
                (*inode)->ext2inode->i_gid = gid;
                fs::idirty(**inode);
                return 0;
            }
            case SYS_umask: {
                const auto new_mask = syscall::GetArgument<1, int>(*tf);
                auto& proc = process::GetCurrent();
                auto old_umask = proc.umask;
                proc.umask = new_mask & modeMask;
                return old_umask;
            }
            case SYS_chmod: {
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                auto mode = syscall::GetArgument<2, int>(*tf);
                auto inode = fs::namei(path.get(), fs::Follow::Yes, {});
                if (!inode) return result::Error(inode.error());
                mode &= modeMask;
                (*inode)->ext2inode->i_mode = ((*inode)->ext2inode->i_mode & ~modeMask) | mode;
                fs::idirty(**inode);
                return 0;
            }
            case SYS_unlink: {
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                return fs::Unlink(path.get());
            }
            case SYS_mkdir: {
                const auto mask = (~process::GetCurrent().umask) & modeMask;
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                const auto mode = syscall::GetArgument<2, int>(*tf);
                return fs::MakeDirectory(path.get(), mode & mask);
            }
            case SYS_rmdir: {
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                return fs::RemoveDirectory(path.get());
            }
            case SYS_fchown: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                if (file->f_inode == nullptr || file->f_inode->ext2inode == nullptr)
                    return result::Error(error::Code::NoEntry);
                const auto uid = syscall::GetArgument<2, int>(*tf);
                const auto gid = syscall::GetArgument<3, int>(*tf);
                file->f_inode->ext2inode->i_uid = uid;
                file->f_inode->ext2inode->i_gid = gid;
                fs::idirty(*file->f_inode);
                return 0;
            }
            case SYS_fchmod: {
                const auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr)
                    return result::Error(error::Code::BadFileHandle);
                if (file->f_inode == nullptr || file->f_inode->ext2inode == nullptr)
                    return result::Error(error::Code::NoEntry);
                auto mode = syscall::GetArgument<2, int>(*tf);
                mode &= modeMask;
                file->f_inode->ext2inode->i_mode =
                    (file->f_inode->ext2inode->i_mode & ~modeMask) | mode;
                fs::idirty(*file->f_inode);
                return 0;
            }
            case SYS_link: {
                const auto oldPath = syscall::GetArgument<1, const char*>(*tf);
                const auto newPath = syscall::GetArgument<2, const char*>(*tf);
                return fs::Link(oldPath.get(), newPath.get());
            }
            case SYS_readlink: {
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                auto buf = syscall::GetArgument<2, char*>(*tf);
                const auto size = syscall::GetArgument<3, size_t>(*tf);
                auto inode = fs::namei(path.get(), fs::Follow::No, {});
                if (!inode) return result::Error(inode.error());
                if (((*inode)->ext2inode->i_mode & EXT2_S_IFMASK) != EXT2_S_IFLNK) {
                    return result::Error(error::Code::InvalidArgument);
                }
                return fs::Read(**inode, buf.get(), 0, size);
            }
            case SYS_symlink: {
                const auto oldPath = syscall::GetArgument<1, const char*>(*tf);
                const auto newPath = syscall::GetArgument<2, const char*>(*tf);
                return fs::SymLink(oldPath.get(), newPath.get());
            }
            case SYS_procinfo: {
                return process::ProcInfo(*tf);
            }
            case SYS_uname: {
                auto utsBuf = syscall::GetArgument<1, utsname*>(*tf);
                utsname uts{};
                strlcpy(uts.sysname, "dogfood", sizeof(uts.sysname));
                strlcpy(uts.nodename, "localhost", sizeof(uts.nodename));
                strlcpy(uts.release, "[git hash here]", sizeof(uts.release));
                strlcpy(uts.version, "0.2", sizeof(uts.version));
                strlcpy(uts.machine, "x86_64", sizeof(uts.machine));
                return utsBuf.Set(uts);
            }
            case SYS_ptrace: {
                return ptrace::PTrace(*tf);
            }
            case SYS_sigprocmask: {
                return signal::sigprocmask(*tf);
            }
            case SYS_pipe: {
                return pipe::pipe(*tf);
            }
            case SYS_select: {
                return select::Select(*tf);
            }
            case SYS_mknod: {
                const auto path = syscall::GetArgument<1, const char*>(*tf);
                const auto mode = syscall::GetArgument<2, mode_t>(*tf);
                const auto dev = syscall::GetArgument<3, dev_t>(*tf);
                return fs::Mknod(path.get(), mode, dev);
            }
        }
        Print("[", process::GetCurrent().pid, "] unsupported syscall ",
            syscall::GetNumber(*tf), " " , syscall::GetArgument<1>(*tf),
            " [ ", syscall::GetArgument<2>(*tf), " ", syscall::GetArgument<3>(*tf),
            " ", syscall::GetArgument<4>(*tf), " ", syscall::GetArgument<5>(*tf),
            " ", syscall::GetArgument<6>(*tf), "]\n");
        return result::Error(error::Code::BadSystemCall);
    }
} // namespace

extern "C" uint64_t perform_syscall(amd64::TrapFrame* tf)
{
    auto& current = process::GetCurrent();
    if (current.ptrace.traced && current.ptrace.traceSyscall) {
        current.ptrace.signal = SIGTRAP;
        current.state = process::State::Stopped;
        signal::Send(*current.parent, SIGCHLD);
        process::Yield();
    }

    const auto result = MapResult(DoSyscall(tf));
    if (current.ptrace.traced && current.ptrace.traceSyscall) {
        tf->rax = result; // so that the tracer can see it

        current.ptrace.signal = SIGTRAP;
        current.state = process::State::Stopped;
        signal::Send(*current.parent, SIGCHLD);
        process::Yield();
    }
    return result;
}
