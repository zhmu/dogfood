#pragma once

#include "types.h"

namespace fs
{
    struct Inode;
}
namespace process
{
    struct Process;
}
namespace pipe
{
    struct Pipe;
}

namespace file
{
    struct File {
        int f_refcount = 0;
        int f_flags = 0;
        bool f_console = false;
        fs::Inode* f_inode = nullptr;
        pipe::Pipe* f_pipe = nullptr;
        off_t f_offset = 0;
    };

    File* Allocate(process::Process& proc);
    void Free(File&);
    void CloneTable(const process::Process& parent, process::Process& child);
    File* FindByIndex(process::Process& proc, int fd);
    File* AllocateByIndex(process::Process& proc, int fd);
    void Dup(const File& source, File& dest);

    int Write(File& file, const void* buf, int len);
    int Read(File& file, void* buf, int len);

    bool CanRead(File& file);
    bool CanWrite(File& file);
    bool HasError(File& file);

} // namespace file
