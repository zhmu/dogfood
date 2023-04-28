#pragma once

#include "types.h"
#include "result.h"
#include <dogfood/stat.h>
#include <memory>
#include <optional>

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

    namespace detail {
        void PutInode(Inode&);

        struct InodeDeref {
            void operator()(Inode* ptr) { PutInode(*ptr); }
        };
    }
    using InodeRef = std::unique_ptr<Inode, detail::InodeDeref>;

    struct DEntry {
        InodeNumber d_ino = 0;
        char d_name[MaxDirectoryEntryNameLength] = {};
    };

    enum class Follow { No, Yes };

    void Initialize();
    void MountRootFileSystem();
    result::MaybeInt Sync(Inode* inode);
    result::MaybeInt Read(Inode& inode, void* dst, off_t offset, unsigned int count);
    result::MaybeInt Write(Inode& inode, const void* dst, off_t offset, unsigned int count);

    result::Maybe<InodeRef> iget(Device dev, InodeNumber inum);
    [[nodiscard]] InodeRef ReferenceInode(InodeRef&);
    void idirty(Inode& inode);
    result::Maybe<InodeRef> namei(const char* path, const Follow follow, std::optional<InodeRef> parent_inode);
    result::Maybe<stat> Stat(Inode& inode);
    result::MaybeInt ResolveDirectoryName(Inode& inode, char* buffer, int bufferSize);
    result::MaybeInt Link(const char* source, const char* dest);
    result::MaybeInt SymLink(const char* source, const char* dest);
    result::MaybeInt Mknod(const char* path, mode_t mode, dev_t dev);

    result::Maybe<InodeRef> Open(const char* path, int flags, int mode);
    result::MaybeInt MakeDirectory(const char* path, int mode);
    result::MaybeInt RemoveDirectory(const char* path);
    result::MaybeInt Unlink(const char* path);

    void CloneTable(const process::Process& parent, process::Process& child);

    result::Maybe<InodeRef> CreateRegular(Inode& parent, const char* name, int mode);
} // namespace fs
