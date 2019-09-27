#pragma once

#include "types.h"

namespace ext2
{
    struct Inode;
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
    Inode* namei(const char* path);
} // namespace fs
