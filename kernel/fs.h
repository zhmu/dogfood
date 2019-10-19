#pragma once

#include "types.h"

namespace ext2
{
    struct Inode;
}

namespace process
{
    struct Process;
}

namespace fs
{
    using Device = int;
    using InodeNumber = uint32_t;
    inline constexpr unsigned int MaxPathLength = 256;
    inline constexpr unsigned int MaxDirectoryEntryNameLength = 64;

    struct Inode {
        Device dev = 0;
        InodeNumber inum = 0;
        int refcount = 0;
        ext2::Inode* ext2inode = nullptr;
    };

    struct DEntry {
        InodeNumber d_ino;
        char d_name[MaxDirectoryEntryNameLength];
    };

    void Initialize();
    int Read(Inode& inode, void* dst, off_t offset, unsigned int count);

    Inode* iget(Device dev, InodeNumber inum);
    void iput(Inode& inode);
    void iref(Inode& inode);
    Inode* namei(const char* path);

    void CloneTable(const process::Process& parent, process::Process& child);
} // namespace fs
