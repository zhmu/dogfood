#pragma once

#include "types.h"
#include "fs.h"
#include "result.h"

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
        fs::InodeRef f_inode;
        device::CharacterDevice* f_chardev = nullptr;
        pipe::Pipe* f_pipe = nullptr;
        off_t f_offset = 0;
    };

    File* Allocate(process::Process& proc);
    File* AllocateConsole(process::Process& proc);
    void Free(File&);
    void CloneTable(process::Process& parent, process::Process& child);
    File* FindByIndex(process::Process& proc, int fd);
    File* AllocateByIndex(process::Process& proc, int fd);
    void Dup(File& source, File& dest);

    result::MaybeInt Open(process::Process& proc, fs::InodeRef inode, int flags);

    result::MaybeInt Write(File& file, const void* buf, int len);
    result::MaybeInt Read(File& file, void* buf, int len);

    bool CanRead(File& file);
    bool CanWrite(File& file);
    bool HasError(File& file);

} // namespace file
