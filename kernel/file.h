#pragma once

#include "types.h"
#include "error.h"
#include <expected>

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

namespace device
{
    struct CharacterDevice;
}

namespace file
{
    struct File {
        bool f_in_use = false;
        int f_flags = 0;
        fs::Inode* f_inode = nullptr;
        device::CharacterDevice* f_chardev = nullptr;
        pipe::Pipe* f_pipe = nullptr;
        off_t f_offset = 0;
    };

    File* Allocate(process::Process& proc);
    File* AllocateConsole(process::Process& proc);
    void Free(File&);
    void CloneTable(const process::Process& parent, process::Process& child);
    File* FindByIndex(process::Process& proc, int fd);
    File* AllocateByIndex(process::Process& proc, int fd);
    void Dup(const File& source, File& dest);

    std::expected<int, error::Code> Open(process::Process& proc, fs::Inode& inode, int flags);

    int Write(File& file, const void* buf, int len);
    int Read(File& file, void* buf, int len);

    bool CanRead(File& file);
    bool CanWrite(File& file);
    bool HasError(File& file);

} // namespace file
