#pragma once

#include "types.h"
#include "result.h"

struct stat;

namespace ext2::on_disk
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
        ext2::on_disk::Inode* ext2inode = nullptr;
    };

    struct DEntry {
        InodeNumber d_ino = 0;
        char d_name[MaxDirectoryEntryNameLength] = {};
    };

    enum class Follow { No, Yes };

    void Initialize();
    void MountRootFileSystem();
    result::MaybeInt Read(Inode& inode, void* dst, off_t offset, unsigned int count);
    result::MaybeInt Write(fs::Inode& inode, const void* dst, off_t offset, unsigned int count);

    result::Maybe<Inode*> iget(Device dev, InodeNumber inum);
    void iput(Inode& inode);
    void iref(Inode& inode);
    void idirty(Inode& inode);
    Inode* namei(const char* path, const Follow follow, fs::Inode* parent_inode = nullptr);
    bool Stat(Inode& inode, stat& sbuf);
    result::MaybeInt ResolveDirectoryName(Inode& inode, char* buffer, int bufferSize);
    result::MaybeInt Link(const char* source, const char* dest);
    result::MaybeInt SymLink(const char* source, const char* dest);
    result::MaybeInt Mknod(const char* path, mode_t mode, dev_t dev);

    result::Maybe<Inode*> Open(const char* path, int flags, int mode);
    result::MaybeInt MakeDirectory(const char* path, int mode);
    result::MaybeInt RemoveDirectory(const char* path);
    result::MaybeInt Unlink(const char* path);

    void CloneTable(const process::Process& parent, process::Process& child);
} // namespace fs
