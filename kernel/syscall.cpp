#include "types.h"
#include "syscall.h"
#include "amd64.h"
#include "console.h"
#include "exec.h"
#include "file.h"
#include "errno.h"
#include "ext2.h"
#include "process.h"
#include "lib.h"
#include "vm.h"

#include "stat.h"

#define DEBUG_SYSCALL 0

namespace
{
    uint64_t DoSyscall(amd64::TrapFrame* tf)
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
            case SYS_open: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto flags = static_cast<int>(syscall::GetArgument<2>(*tf));
                auto mode = static_cast<int>(syscall::GetArgument<3>(*tf));
                // XXX only support opening for now
                auto inode = fs::namei(path);
                if (inode == nullptr) return -ENOENT;

                auto& current = process::GetCurrent();
                auto file = file::Allocate(current);
                if (file == nullptr) { fs::iput(*inode); return -ENFILE; }
                file->f_inode = inode;
                return file - &current.files[0];
                break;
            }
            case SYS_close: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr) return -EBADF;
                file::Free(*file);
                return 0;
            }
            case SYS_stat:
            case SYS_lstat: {
                auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(*tf));
                auto buf = reinterpret_cast<stat*>(syscall::GetArgument<2>(*tf));

                auto inode = fs::namei(path);
                if (inode == nullptr) return -ENOENT;
                auto ret = fs::Stat(*inode, *buf);
                fs::iput(*inode);
                return ret ? 0 : -EIO;
            }
            case SYS_fstat: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                auto buf = reinterpret_cast<stat*>(syscall::GetArgument<2>(*tf));
                if (file == nullptr) return -EBADF;
                if (file->f_inode == nullptr) {
                    memset(buf, 0, sizeof *buf);
                    return 0;
                }
                return fs::Stat(*file->f_inode, *buf) ? 0 : -EIO;
            }
            case SYS_seek: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                auto offset = reinterpret_cast<long*>(syscall::GetArgument<2>(*tf));
                auto whence = syscall::GetArgument<3>(*tf);
                if (file == nullptr) return -EBADF;
                if (file->f_inode == nullptr || file->f_inode->ext2inode == nullptr) return -ESPIPE;
                long new_offset = file->f_offset;
                auto file_size = file->f_inode->ext2inode->i_size;
                switch(whence) {
                    case SEEK_SET: new_offset = *offset; break;
                    case SEEK_CUR: new_offset += *offset; break;
                    case SEEK_END: new_offset = file->f_inode->ext2inode->i_size - *offset; break;
                }
                if(new_offset < 0) new_offset = 0;
                if(new_offset > file_size) new_offset = file_size;
                file->f_offset = new_offset;
                *offset = new_offset;
                return 0;
            }
            case SYS_dup: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr) return -EBADF;
                auto& current = process::GetCurrent();
                auto file2 = file::Allocate(current);
                if (file2 == nullptr) return -ENFILE;
                *file2 = *file;
                if (file2->f_inode != nullptr)
                    fs::iref(*file2->f_inode);
                return file2 - &current.files[0];

            }
            case SYS_fcntl: {
                auto file = file::FindByIndex(process::GetCurrent(), syscall::GetArgument<1>(*tf));
                if (file == nullptr) return -EBADF;
                auto op = syscall::GetArgument<2>(*tf);
                switch(op) {
                    case F_GETFL:
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
                return -fs::ResolveDirectoryName(*current.cwd, buf, len);
            }
            case SYS_chdir: {
                auto buf = reinterpret_cast<char*>(syscall::GetArgument<1>(*tf));
                auto& current = process::GetCurrent();
                auto inode = fs::namei(buf);
                if (inode == nullptr) return -ENOENT;
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
                if (file == nullptr) return -EBADF;
                if ((file->f_inode->ext2inode->i_mode & EXT2_S_IFDIR) == 0) return -ENOTDIR;

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
        }
        printf(
            "[%d] unsupported syscall %d %lx [%x %x %x %x %x %x]\n", process::GetCurrent().pid,
            syscall::GetNumber(*tf), syscall::GetArgument<1>(*tf), syscall::GetArgument<2>(*tf),
            syscall::GetArgument<3>(*tf), syscall::GetArgument<4>(*tf), syscall::GetArgument<5>(*tf),
            syscall::GetArgument<6>(*tf));
        return -1;
    }
}

extern "C" uint64_t perform_syscall(amd64::TrapFrame* tf)
{
#if DEBUG_SYSCALL
    printf(
        "[%d] syscall %d %lx [%x %x %x %x %x %x] ->", process::GetCurrent().pid,
        syscall::GetNumber(*tf), syscall::GetArgument<1>(*tf), syscall::GetArgument<2>(*tf),
        syscall::GetArgument<3>(*tf), syscall::GetArgument<4>(*tf), syscall::GetArgument<5>(*tf),
        syscall::GetArgument<6>(*tf));
#endif
    const auto result = DoSyscall(tf);
#if DEBUG_SYSCALL
    printf(" %ld\n", result);
#endif
    return result;
}
