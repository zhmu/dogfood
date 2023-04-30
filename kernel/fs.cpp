#include "fs.h"
#include "bio.h"
#include "ext2.h"
#include "process.h"
#include "lib.h"
#include "debug.h"
#include <dogfood/errno.h>
#include <dogfood/fcntl.h>

namespace fs
{
    namespace
    {
        constexpr debug::Trace<false> Debug;
        constexpr inline Device rootDeviceNumber = 2;
        constexpr inline int maxSymLinkDepth = 10;

        namespace cache
        {
            inline constexpr unsigned int NumberOfInodes = 40;
            Inode inode[NumberOfInodes];
            ext2::on_disk::Inode ext2inode[NumberOfInodes];
        } // namespace cache

        InodeRef rootInode;

        result::MaybeInt FollowSymLink(InodeRef& parent, InodeRef& inode, int& depth);

        bool IsolatePathComponent(const char*& path, std::array<char, MaxDirectoryEntryNameLength>& component)
        {
            while (*path == '/')
                ++path;
            if (*path == '\0')
                return false;

            auto componentBegin = path;
            while (*path != '/' && *path != '\0')
                ++path;
            const auto len = path - componentBegin;
            assert(len + 1 < component.size());
            memcpy(component.data(), componentBegin, len);
            component[len] = '\0';
            return true;
        }

        bool IsDirectoryEmpty(fs::Inode& inode)
        {
            off_t offset = 0;
            fs::DEntry dentry;
            while (ext2::ReadDirectory(inode, offset, dentry)) {
                if (strcmp(dentry.d_name, ".") != 0 && strcmp(dentry.d_name, "..") != 0)
                    return false;
            }
            return true;
        }

        bool IsSymLink(const Inode& inode)
        {
            return (inode.ext2inode->i_mode & EXT2_S_IFMASK) == EXT2_S_IFLNK;
        }

        result::Maybe<InodeRef> LookupInDirectory(Inode& inode, const char* item)
        {
            off_t offset = 0;
            fs::DEntry dentry;
            while (ext2::ReadDirectory(inode, offset, dentry)) {
                if (strcmp(dentry.d_name, item) == 0) {
                    return iget(inode.dev, dentry.d_ino);
                }
            }
            return result::Error(error::Code::NoEntry);
        }

        struct LookupResult {
            InodeRef inode;
            InodeRef parent;
            std::array<char, MaxDirectoryEntryNameLength> component{};
        };

        result::Maybe<LookupResult> Lookup(InodeRef current_inode, const char* path, const Follow follow, int& depth)
        {
            assert(current_inode);
            LookupResult result;
            while (IsolatePathComponent(path, result.component)) {
                if (const auto r = FollowSymLink(result.parent, current_inode, depth); !r) {
                    return result::Error(r.error());
                }
                auto lookup_inode = LookupInDirectory(*current_inode, result.component.data());
                if (!lookup_inode) {
                    if (strchr(path, '/') == nullptr) {
                        // Couldn't find the final piece - return the parent
                        result.parent = std::move(current_inode);
                    }
                    return result;
                }
                result.parent = std::move(current_inode);
                current_inode = std::move(*lookup_inode);
            }

            if (follow == Follow::Yes && IsSymLink(*current_inode)) {
                if (const auto r = FollowSymLink(result.parent, current_inode, depth); !r)
                    return result::Error(r.error());
            }

            result.inode = std::move(current_inode);
            return result;
        }

        result::MaybeInt FollowSymLink(InodeRef& parent, InodeRef& inode, int& depth)
        {
            if (!IsSymLink(*inode)) return 0;
            ++depth;
            if (depth == maxSymLinkDepth) return result::Error(error::Code::LoopDetected);

            char symlink[MaxPathLength];
            const auto result = fs::Read(*inode, symlink, 0, sizeof(symlink) - 1);
            if (!result || *result == 0) return result::Error(error::Code::IOError);
            symlink[*result] = '\0';

            auto next = Lookup(ReferenceInode(parent), symlink, Follow::Yes, depth);
            if (!next) return result::Error(next.error());
            if (next->inode == nullptr) return result::Error(error::Code::NoEntry);
            parent = std::move(next->parent);
            inode = std::move(next->inode);
            return 0;
        }

        result::Maybe<LookupResult> namei2(const char* path, const Follow follow, std::optional<fs::InodeRef> lookup_root)
        {
            fs::InodeRef base_inode;
            if (path[0] == '/') {
                base_inode = fs::ReferenceInode(rootInode);
            } else if (lookup_root) {
                base_inode = std::move(*lookup_root);
            } else {
                base_inode = fs::ReferenceInode(process::GetCurrent().cwd);
            }
            int depth = 0;
            return Lookup(std::move(base_inode), path, follow, depth);
        }

        bool LookupInodeByNumber(Inode& inode, ino_t inum, fs::DEntry& dentry)
        {
            off_t offset = 0;
            while (ext2::ReadDirectory(inode, offset, dentry)) {
                if (strcmp(dentry.d_name, ".") == 0)
                    continue;
                if (strcmp(dentry.d_name, "..") == 0)
                    continue;
                if (dentry.d_ino == inum)
                    return true;
            }
            return false;
        }

        bool CommitInode(fs::Inode& inode)
        {
            if (!inode.dirty) return false;
            ext2::WriteInode(inode);
            inode.dirty = false;
            return true;
        }
    } // namespace

    void Initialize()
    {
        for (size_t n = 0; n < cache::NumberOfInodes; ++n) {
            cache::inode[n].ext2inode = &cache::ext2inode[n];
        }
    }

    void MountRootFileSystem()
    {
        auto inode = ext2::Mount(rootDeviceNumber);
        if (!inode)
            panic("cannot mount root filesystem");
        rootInode = std::move(*inode);
    }

    result::Maybe<InodeRef> iget(Device dev, InodeNumber inum)
    {
        Inode* available = nullptr;
        for (auto& inode : cache::inode) {
            if (inode.refcount == 0 && !available) {
                if (CommitInode(inode)) {
                    // CommitInode() may sleep, and upon returning the
                    // inode might not be available anymore
                    continue;
                }
                available = &inode;
            }
            if (inode.dev != dev || inode.inum != inum)
                continue;
            ++inode.refcount;
            return InodeRef{ &inode };
        }

        if (available == nullptr)
            return result::Error(error::Code::NoFile);
        assert(available->refcount == 0);
        available->dev = dev;
        available->inum = inum;
        available->refcount = 1;
        available->dirty = false;

        ext2::ReadInode(dev, inum, *available->ext2inode);
        return InodeRef{ available };
    }

    namespace detail {
        void PutInode(Inode& inode)
        {
            assert(inode.refcount > 0);
            --inode.refcount;
        }
    }

    void idirty(Inode& inode)
    {
        assert(inode.refcount > 0);
        inode.dirty = true;
    }

    result::MaybeInt Read(fs::Inode& inode, void* dst, off_t offset, unsigned int count)
    {
        if (offset > inode.ext2inode->i_size || offset + count < offset)
            return 0;
        if (offset + count > inode.ext2inode->i_size) {
            count = inode.ext2inode->i_size - offset;
        }

        auto d = reinterpret_cast<char*>(dst);
        while (count > 0) {
            int chunkLen = count;
            if (offset % bio::BlockSize) {
                chunkLen = bio::BlockSize - (offset % bio::BlockSize);
                if (chunkLen > count)
                    chunkLen = count;
            }
            if (chunkLen > bio::BlockSize)
                chunkLen = bio::BlockSize;

            const uint32_t blockNr = ext2::bmap(inode, offset / bio::BlockSize, false);
            {
                auto buf = bio::ReadBlock(inode.dev, blockNr);
                assert(buf);
                memcpy(d, buf->data + (offset % bio::BlockSize), chunkLen);
            }

            d += chunkLen;
            offset += chunkLen;
            count -= chunkLen;
        }
        return d - reinterpret_cast<char*>(dst);
    }

    result::MaybeInt Write(fs::Inode& inode, const void* src, off_t offset, unsigned int count)
    {
        const auto newSize = offset + count;
        auto s = reinterpret_cast<const char*>(src);
        while (count > 0) {
            int chunkLen = count;
            if (offset % bio::BlockSize) {
                chunkLen = bio::BlockSize - (offset % bio::BlockSize);
                if (chunkLen > count)
                    chunkLen = count;
            }
            if (chunkLen > bio::BlockSize)
                chunkLen = bio::BlockSize;

            const uint32_t blockNr = ext2::bmap(inode, offset / bio::BlockSize, true);
            auto buf = bio::ReadBlock(inode.dev, blockNr);
            assert(buf);
            memcpy(buf->data + (offset % bio::BlockSize), s, chunkLen);
            bio::WriteBlock(std::move(buf));

            s += chunkLen;
            offset += chunkLen;
            count -= chunkLen;
        }

        if (newSize > inode.ext2inode->i_size) {
            inode.ext2inode->i_size = newSize;
            fs::idirty(inode);
        }
        return s - reinterpret_cast<const char*>(src);
    }

    result::Maybe<InodeRef> namei(const char* path, const Follow follow, std::optional<fs::InodeRef> parent_inode)
    {
        auto result = namei2(path, follow, std::move(parent_inode));
        if (!result) return result::Error(result.error());
        if (!result->inode)
             return result::Error(error::Code::NoEntry);
        return std::move(result->inode);
    }

    result::Maybe<InodeRef> Open(const char* path, int flags, int mode)
    {
        auto lookup_result = namei2(path, Follow::No /* ? */, {});
        if (!lookup_result) return result::Error(lookup_result.error());
        if (lookup_result->inode) {
            if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
                return result::Error(error::Code::AlreadyExists);
            }
            if (flags & O_TRUNC) {
                ext2::Truncate(*lookup_result->inode);
            }
            return std::move(lookup_result->inode);
        }
        if (lookup_result->parent == nullptr)
            return result::Error(error::Code::NoEntry);
        if ((flags & O_CREAT) == 0)
            return result::Error(error::Code::NoEntry);

        return ext2::CreateRegular(*lookup_result->parent, lookup_result->component.data(), mode);
    }

    result::MaybeInt Unlink(const char* path)
    {
        auto lookup_result = namei2(path, Follow::No, {});
        if (!lookup_result) return result::Error(lookup_result.error());
        if (!lookup_result->inode) {
            return result::Error(error::Code::NoEntry);
        }

        if ((lookup_result->inode->ext2inode->i_mode & EXT2_S_IFMASK) == EXT2_S_IFDIR) {
            return result::Error(error::Code::PermissionDenied);
        }

        if (auto result = ext2::Unlink(*lookup_result->parent, lookup_result->component.data()); !result) {
            return result::Error(result.error());
        }
        return ext2::UnlinkInode(std::move(lookup_result->inode));
    }

    result::MaybeInt Link(const char* source, const char* dest)
    {
        auto source_inode = namei(source, Follow::Yes, {});
        if (!source_inode) return result::Error(source_inode.error());

        auto lookup_result = namei2(dest, Follow::Yes, {});
        if (!lookup_result) return result::Error(lookup_result.error());
        if (lookup_result->inode) {
            return result::Error(error::Code::AlreadyExists);
        }
        if (!lookup_result->parent)
            return result::Error(error::Code::NoEntry);

        return ext2::CreateLink(*lookup_result->parent, **source_inode, lookup_result->component.data());
    }

    result::MaybeInt SymLink(const char* source, const char* dest)
    {
        Inode* parent;
        auto lookup_result = namei2(dest, Follow::Yes, {});
        if (!lookup_result) return result::Error(lookup_result.error());
        if (lookup_result->inode) {
            return result::Error(error::Code::AlreadyExists);
        }
        if (!lookup_result->parent)
            return result::Error(error::Code::NoEntry);

        auto result = ext2::CreateSymlink(*lookup_result->parent, lookup_result->component.data(), dest);
        if (!result) return result::Error(result.error());
        return 0;
    }

    result::MaybeInt MakeDirectory(const char* path, int mode)
    {
        auto lookup_result = namei2(path, Follow::Yes, {});
        if (!lookup_result) return result::Error(lookup_result.error());
        if (lookup_result->inode) {
            return result::Error(error::Code::AlreadyExists);
        }
        if (!lookup_result->parent)
            return result::Error(error::Code::NoEntry);

        return ext2::CreateDirectory(*lookup_result->parent, lookup_result->component.data(), mode);
    }

    result::MaybeInt RemoveDirectory(const char* path)
    {
        auto lookup_result = namei2(path, Follow::Yes, {});
        if (!lookup_result) return result::Error(lookup_result.error());
        if (!lookup_result->inode) {
            return result::Error(error::Code::NoEntry);
        }

        if ((lookup_result->inode->ext2inode->i_mode & EXT2_S_IFMASK) != EXT2_S_IFDIR) {
            return result::Error(error::Code::NotADirectory);
        }

        if (!IsDirectoryEmpty(*lookup_result->inode)) {
            return result::Error(error::Code::NotEmpty);
        }

        if (!ext2::Unlink(*lookup_result->parent, lookup_result->component.data())) {
            return result::Error(error::Code::IOError);
        }
        return ext2::RemoveDirectory(*lookup_result->parent, std::move(lookup_result->inode));
    }

    result::MaybeInt ResolveDirectoryName(InodeRef& inode, char* buffer, int bufferSize)
    {
        if (inode->ext2inode == nullptr || (inode->ext2inode->i_mode & EXT2_S_IFDIR) == 0)
            return result::Error(error::Code::NotADirectory);
        if (bufferSize < 2)
            return result::Error(error::Code::NameTooLong);

        auto current = ReferenceInode(inode);

        int currentPosition = bufferSize - 1;
        buffer[currentPosition] = '\0';
        while (current.get() != rootInode.get()) {
            auto lookup_inode = LookupInDirectory(*current, "..");
            if (!lookup_inode) break;
            auto parent = std::move(*lookup_inode);

            // Find the current inode's name
            fs::DEntry dentry;
            if (!LookupInodeByNumber(*parent, current->inum, dentry)) {
                return result::Error(error::Code::NoEntry);
            }

            const int entryLength = strlen(dentry.d_name);
            if (currentPosition - entryLength <= 0) {
                return result::Error(error::Code::NameTooLong);
            }
            memcpy(buffer + currentPosition - entryLength, dentry.d_name, entryLength);
            buffer[currentPosition - entryLength - 1] = '/';
            currentPosition -= entryLength + 1;

            current = std::move(parent);
        }
        if (currentPosition == bufferSize - 1) {
            strlcpy(buffer, "/", bufferSize);
        } else {
            for (int n = 0; n < bufferSize - currentPosition; n++) {
                buffer[n] = buffer[currentPosition + n];
            }
            buffer[bufferSize - currentPosition] = '\0';
        }
        return 0;
    }

    result::Maybe<stat> Stat(Inode& inode)
    {
        if (inode.ext2inode == nullptr)
            return result::Error(error::Code::IOError);
        return ext2::Stat(inode);
    }

    result::MaybeInt Mknod(const char* path, mode_t mode, dev_t dev)
    {
        auto lookup_result = namei2(path, Follow::Yes, {});
        if (!lookup_result) return result::Error(lookup_result.error());
        if (lookup_result->inode) {
            return result::Error(error::Code::AlreadyExists);
        }
        if (!lookup_result->parent)
            return result::Error(error::Code::NoEntry);

        const auto type = mode & EXT2_S_IFMASK;
        if (type != EXT2_S_IFBLK && type != EXT2_S_IFCHR) {
            return result::Error(error::Code::InvalidArgument);
        }

        const auto result = ext2::CreateSpecial(*lookup_result->parent, lookup_result->component.data(), mode, dev);
        if (result) {
            return 0;
        }
        return result::Error(result.error());
    }

    [[nodiscard]] InodeRef ReferenceInode(InodeRef& inode)
    {
        if(!inode) { return {}; }
        assert(inode->refcount > 0);
        ++inode->refcount;
        return InodeRef{ inode.get() };
    }

    result::MaybeInt Sync(fs::Inode* inode)
    {
        if (inode) {
            CommitInode(*inode);
            return 0;
        }
        int n = 0;
        for (auto& inode : cache::inode) {
            if (CommitInode(inode)) ++n;
        }
        const auto m = bio::Sync();
        Debug("fs::Sync: ", n, " inodes and ", m, " bios\n");
        return 0;
    }
} // namespace fs
