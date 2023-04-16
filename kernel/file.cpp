#include "file.h"
#include "process.h"
#include "fs.h"
#include "pipe.h"
#include "lib.h"
#include <dogfood/device.h>
#include <dogfood/errno.h>
#include <dogfood/fcntl.h>

#include "device.h"
#include "ext2.h"

namespace file
{
    File* Allocate(process::Process& proc)
    {
        for (auto& file : proc.files) {
            if (file.f_in_use)
                continue;
            file = File{};
            file.f_in_use = true;
            return &file;
        }
        return nullptr;
    }

    File* AllocateConsole(process::Process& proc)
    {
        auto file = Allocate(proc);
        if (file != nullptr) {
            file->f_chardev = device::LookupConsole();
        }
        return file;
    }

    void Free(File& file)
    {
        if (!file.f_in_use) return;

        if (file.f_inode != nullptr)
            fs::iput(*file.f_inode);

        if (file.f_pipe != nullptr) {
            if (file.f_flags & O_RDONLY)
                --file.f_pipe->p_num_readers;
            else if (file.f_flags & O_WRONLY)
                --file.f_pipe->p_num_writers;
            else
                assert(0);
            process::Wakeup(file.f_pipe);
        }

        file = File{};
    }

    void Dup(const File& source, File& dest)
    {
        Free(dest);
        dest = source;
        if (dest.f_inode != nullptr)
            fs::iref(*dest.f_inode);
        if (dest.f_pipe != nullptr) {
            if (dest.f_flags & O_RDONLY)
                ++dest.f_pipe->p_num_readers;
            else if (dest.f_flags & O_WRONLY)
                ++dest.f_pipe->p_num_writers;
        }
        dest.f_flags &= ~O_CLOEXEC;
    }

    void CloneTable(const process::Process& parent, process::Process& child)
    {
        for (int n = 0; n < process::maxFiles; ++n) {
            const auto& parentFile = parent.files[n];
            if (!parentFile.f_in_use)
                continue;
            if ((parentFile.f_flags & O_CLOEXEC) != 0)
                continue;

            auto& childFile = child.files[n];
            file::Dup(parentFile, childFile);
        }
    }

    result::MaybeInt Write(File& file, const void* buf, int len)
    {
        if (file.f_chardev)
            return file.f_chardev->Write(buf, len);
        if (file.f_pipe)
            return file.f_pipe->Write(buf, len);

        const auto result = fs::Write(*file.f_inode, buf, file.f_offset, len);
        if (result) file.f_offset += *result;
        return result;
    }

    result::MaybeInt Read(File& file, void* buf, int len)
    {
        if (file.f_chardev)
            return file.f_chardev->Read(buf, len);
        if (file.f_pipe)
            return file.f_pipe->Read(buf, len, (file.f_flags & O_NONBLOCK) != 0);

        const auto result = fs::Read(*file.f_inode, buf, file.f_offset, len);
        if (result) file.f_offset += *result;
        return result;
    }

    bool CanRead(File& file)
    {
        if (file.f_pipe)
            return file.f_pipe->CanRead();
        if (file.f_chardev)
            return file.f_chardev->CanRead();
        return false;
    }

    bool CanWrite(File& file)
    {
        if (file.f_pipe)
            return file.f_pipe->CanWrite();
        if (file.f_chardev)
            return file.f_chardev->CanWrite();
        return false;
    }

    bool HasError(File& file)
    {
        return false;
    }

    File* FindByIndex(process::Process& proc, int fd)
    {
        if (fd < 0 || fd >= process::maxFiles)
            return nullptr;
        File& file = proc.files[fd];
        if (!file.f_in_use)
            return nullptr;
        return &file;
    }

    File* AllocateByIndex(process::Process& proc, int fd)
    {
        if (fd < 0 || fd >= process::maxFiles)
            return nullptr;
        File& file = proc.files[fd];
        Free(file);
        return &file;
    }

    result::MaybeInt Open(process::Process& proc, fs::Inode& inode, int flags)
    {
        auto file = file::Allocate(proc);
        if (file == nullptr)
            return result::Error(error::Code::NoFile);

        const auto type = inode.ext2inode->i_mode & EXT2_S_IFMASK;
        switch(type) {
            case EXT2_S_IFBLK:
                file::Free(*file);
                return result::Error(error::Code::NoDevice);
            case EXT2_S_IFCHR: {
                const auto dev = inode.ext2inode->i_block[0];
                file::Free(*file);
                auto char_dev = device::LookupCharacterDevice(dev);
                if (!char_dev) return result::Error(error::Code::NoDevice);
                file->f_chardev = char_dev;
                break;
            }
            default:
                file->f_inode = &inode;
                fs::iref(inode);
                break;
        }
        file->f_flags = flags;
        return file - &proc.files[0];
    }

} // namespace file
