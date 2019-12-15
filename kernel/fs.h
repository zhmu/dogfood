#pragma once

#include "types.h"

struct stat;

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
        bool dirty = false;
        ext2::Inode* ext2inode = nullptr;
    };

    struct DEntry {
        InodeNumber d_ino = 0;
        char d_name[MaxDirectoryEntryNameLength] = {};
    };

    void Initialize();
    void MountRootFileSystem();
    int Read(Inode& inode, void* dst, off_t offset, unsigned int count);
    int Write(fs::Inode& inode, const void* dst, off_t offset, unsigned int count);

    Inode* iget(Device dev, InodeNumber inum);
    void iput(Inode& inode);
    void iref(Inode& inode);
    void idirty(Inode& inode);
    Inode* namei(const char* path, const bool follow);
    bool Stat(Inode& inode, stat& sbuf);
    int ResolveDirectoryName(Inode& inode, char* buffer, int bufferSize);
    int Link(const char* source, const char* dest);
    int SymLink(const char* source, const char* dest);

    int Open(const char* path, int flags, int mode, Inode*& inode);
    int MakeDirectory(const char* path, int mode);
    int RemoveDirectory(const char* path);
    int Unlink(const char* path);

    void CloneTable(const process::Process& parent, process::Process& child);
} // namespace fs
