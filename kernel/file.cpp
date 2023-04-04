#include "file.h"
#include "process.h"
#include "fs.h"
#include "pipe.h"
#include "lib.h"
#include <dogfood/errno.h>
#include <dogfood/fcntl.h>

#include "hw/console.h"

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

    int Write(File& file, const void* buf, int len)
    {
        if (file.f_console)
            return console::Write(buf, len);
        if (file.f_pipe)
            return file.f_pipe->Write(buf, len);

        const auto count = fs::Write(*file.f_inode, buf, file.f_offset, len);
        file.f_offset += count;
        return count;
    }

    int Read(File& file, void* buf, int len)
    {
        if (file.f_console)
            return console::Read(buf, len);
        if (file.f_pipe)
            return file.f_pipe->Read(buf, len, (file.f_flags & O_NONBLOCK) != 0);

        const auto count = fs::Read(*file.f_inode, buf, file.f_offset, len);
        file.f_offset += count;
        return count;
    }

    bool CanRead(File& file)
    {
        if (file.f_pipe)
            return file.f_pipe->CanRead();
        if (file.f_console)
            return console::CanRead();
        return false;
    }

    bool CanWrite(File& file)
    {
        if (file.f_pipe)
            return file.f_pipe->CanWrite();
        if (file.f_console)
            return console::CanWrite();
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

} // namespace file
