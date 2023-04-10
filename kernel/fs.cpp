#include "fs.h"
#include "bio.h"
#include "ext2.h"
#include "process.h"
#include "lib.h"
#include <dogfood/errno.h>
#include <dogfood/fcntl.h>
#include <dogfood/stat.h>

namespace fs
{
    namespace
    {
        constexpr inline Device rootDeviceNumber = 2;
        constexpr inline int maxSymLinkDepth = 10;

        namespace cache
        {
            inline constexpr unsigned int NumberOfInodes = 40;
            Inode inode[NumberOfInodes];
            ext2::on_disk::Inode ext2inode[NumberOfInodes];
        } // namespace cache

        Inode* rootInode = nullptr;

        std::expected<int, error::Code> FollowSymLink(Inode*& parent, Inode*& inode, int& depth);

        bool IsolatePathComponent(const char*& path, char component[MaxDirectoryEntryNameLength])
        {
            while (*path == '/')
                ++path;
            if (*path == '\0')
                return false;

            auto componentBegin = path;
            while (*path != '/' && *path != '\0')
                ++path;
            int len = path - componentBegin;
            memcpy(component, componentBegin, len);
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

        Inode* LookupInDirectory(Inode& inode, const char* item)
        {
            off_t offset = 0;
            fs::DEntry dentry;
            while (ext2::ReadDirectory(inode, offset, dentry)) {
                if (strcmp(dentry.d_name, item) == 0)
                    return iget(inode.dev, dentry.d_ino);
            }
            return nullptr;
        }

        Inode* Lookup(Inode* current_inode, const char* path, const bool follow, Inode*& parent, char* component, int& depth)
        {
            iref(*current_inode);

            parent = nullptr;
            while (IsolatePathComponent(path, component)) {
                if (FollowSymLink(parent, current_inode, depth) != 0) {
                    return nullptr;
                }
                auto new_inode = LookupInDirectory(*current_inode, component);
                if (new_inode == nullptr) {
                    if (strchr(path, '/') == nullptr) {
                        parent = current_inode;
                    } else
                        iput(*current_inode); // not final piece
                    return nullptr;
                }
                if (parent != nullptr)
                    iput(*parent);
                parent = current_inode;
                current_inode = new_inode;
            }

            if (follow && IsSymLink(*current_inode)) {
                if (auto result = FollowSymLink(parent, current_inode, depth); result != 0)
                    return nullptr;
            }

            return current_inode;
        }

        std::expected<int, error::Code> FollowSymLink(Inode*& parent, Inode*& inode, int& depth)
        {
            if (!IsSymLink(*inode)) return 0;
            ++depth;
            if (depth == maxSymLinkDepth) return std::unexpected(error::Code::LoopDetected);

            char symlink[MaxPathLength];
            const auto n = fs::Read(*inode, symlink, 0, sizeof(symlink) - 1);
            if (n <= 0) return std::unexpected(error::Code::IOError);
            symlink[n] = '\0';

            char component[MaxPathLength];
            Inode* newParent = nullptr;
            auto next = Lookup(parent, symlink, true, newParent, component, depth);
            if (next == nullptr) return std::unexpected(error::Code::NoEntry);
            iput(*parent);
            parent= newParent;
            inode = next;
            return 0;
        }

        Inode* namei2(const char* path, const bool follow, Inode*& parent, char* component, fs::Inode* lookup_root = nullptr)
        {
            if (path[0] == '/')
                lookup_root = rootInode;
            else if (lookup_root == nullptr)
                lookup_root = process::GetCurrent().cwd;
            int depth = 0;
            return Lookup(lookup_root, path, follow, parent, component, depth);
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

    } // namespace

    void Initialize()
    {
        for (size_t n = 0; n < cache::NumberOfInodes; ++n) {
            cache::inode[n].ext2inode = &cache::ext2inode[n];
        }
    }

    void MountRootFileSystem()
    {
        rootInode = ext2::Mount(rootDeviceNumber);
        if (rootInode == nullptr)
            panic("cannot mount root filesystem");
    }

    Inode* iget(Device dev, InodeNumber inum)
    {
        Inode* available = nullptr;
        for (auto& inode : cache::inode) {
            if (inode.refcount == 0) {
                if (available == nullptr)
                    available = &inode;
                break;
            }
            if (inode.dev != dev || inode.inum != inum)
                continue;
            ++inode.refcount;
            return &inode;
        }

        assert(available != nullptr);
        available->dev = dev;
        available->inum = inum;
        available->refcount = 1;
        available->dirty = false;

        ext2::ReadInode(dev, inum, *available->ext2inode);
        return available;
    }

    void iput(Inode& inode)
    {
        assert(inode.refcount > 0);
        if (--inode.refcount == 0 && inode.dirty)
            ext2::WriteInode(inode);
    }

    void iref(Inode& inode)
    {
        assert(inode.refcount > 0);
        ++inode.refcount;
    }

    void idirty(Inode& inode)
    {
        assert(inode.refcount > 0);
        // inode.dirty = true;
        ext2::WriteInode(inode);
    }

    int Read(fs::Inode& inode, void* dst, off_t offset, unsigned int count)
    {
        if (offset > inode.ext2inode->i_size || offset + count < offset)
            return -1;
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

    int Write(fs::Inode& inode, const void* src, off_t offset, unsigned int count)
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

    Inode* namei(const char* path, const bool follow, fs::Inode* parent_inode)
    {
        Inode* parent;
        char component[MaxPathLength];
        auto inode = namei2(path, follow, parent, component, parent_inode);
        if (parent != nullptr)
            iput(*parent);
        if (inode != nullptr)
            return inode;
        return nullptr;
    }

    std::expected<Inode*, error::Code> Open(const char* path, int flags, int mode)
    {
        Inode* parent;
        char component[MaxPathLength];
        auto inode = namei2(path, false /* ? */, parent, component);
        if (inode != nullptr) {
            if (parent != nullptr)
                fs::iput(*parent);
            if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
                return std::unexpected(error::Code::AlreadyExists);
            if (flags & O_TRUNC) {
                ext2::Truncate(*inode);
            }
            return inode;
        }
        if (parent == nullptr)
            return std::unexpected(error::Code::NoEntry);
        if ((flags & O_CREAT) == 0)
            return std::unexpected(error::Code::NoEntry);

        auto inum = ext2::AllocateInode(*parent);
        if (inum == 0)
            return std::unexpected(error::Code::OutOfSpace);

        auto newInode = fs::iget(parent->dev, inum);
        assert(newInode != nullptr);
        {
            auto& e2i = *newInode->ext2inode;
            e2i = {};
            e2i.i_mode = EXT2_S_IFREG | mode;
            e2i.i_links_count = 1;
        }
        fs::idirty(*newInode);

        if (!ext2::AddEntryToDirectory(*parent, inum, EXT2_FT_REG_FILE, component)) {
            // TODO deallocate inode
            return std::unexpected(error::Code::OutOfSpace);
        }
        fs::idirty(*parent);
        fs::iput(*parent);
        return newInode;
    }

    std::expected<int, error::Code> Unlink(const char* path)
    {
        Inode* parent;
        char component[MaxPathLength];
        Inode* inode = namei2(path, false, parent, component);
        if (inode == nullptr) {
            if (parent != nullptr)
                fs::iput(*parent);
            return std::unexpected(error::Code::NoEntry);
        }

        if ((inode->ext2inode->i_mode & EXT2_S_IFMASK) == EXT2_S_IFDIR) {
            fs::iput(*parent);
            fs::iput(*inode);
            return std::unexpected(error::Code::PermissionDenied);
        }

        if (!ext2::RemoveEntryFromDirectory(*parent, component)) {
            fs::iput(*parent);
            fs::iput(*inode);
            return std::unexpected(error::Code::IOError);
        }
        fs::iput(*parent);
        ext2::Unlink(*inode);
        return 0;
    }

    std::expected<int, error::Code> Link(const char* source, const char* dest)
    {
        auto sourceInode = namei(source, true);
        if (sourceInode == nullptr)
            return std::unexpected(error::Code::NoEntry);

        Inode* parent;
        char component[MaxPathLength];
        if (auto inode = namei2(dest, true, parent, component); inode != nullptr) {
            if (parent != nullptr)
                fs::iput(*parent);
            return std::unexpected(error::Code::AlreadyExists);
        }
        if (parent == nullptr)
            return std::unexpected(error::Code::NoEntry);

        // XXX EXT2_FT_REG_FILE should be looked up
        if (!ext2::AddEntryToDirectory(*parent, sourceInode->inum, EXT2_FT_REG_FILE, component)) {
            return std::unexpected(error::Code::OutOfSpace);
        }
        ++sourceInode->ext2inode->i_links_count;
        fs::idirty(*sourceInode);
        fs::iput(*sourceInode);
        fs::iput(*parent);
        return 0;
    }

    std::expected<int, error::Code> SymLink(const char* source, const char* dest)
    {
        Inode* parent;
        char component[MaxPathLength];
        if (auto inode = namei2(dest, true, parent, component); inode != nullptr) {
            if (parent != nullptr)
                fs::iput(*parent);
            return std::unexpected(error::Code::AlreadyExists);
        }
        if (parent == nullptr)
            return std::unexpected(error::Code::NoEntry);

        auto inum = ext2::AllocateInode(*parent);
        if (inum == 0)
            return std::unexpected(error::Code::OutOfSpace);

        auto newInode = fs::iget(parent->dev, inum);
        assert(newInode != nullptr);
        {
            auto& e2i = *newInode->ext2inode;
            e2i = {};
            e2i.i_mode = EXT2_S_IFLNK | 0777;
            e2i.i_links_count = 1;
        }
        fs::idirty(*newInode);
        if (fs::Write(*newInode, source, 0, strlen(source)) != strlen(source)) {
            // XXX undo damage
            fs::iput(*newInode);
            fs::iput(*parent);
            return std::unexpected(error::Code::IOError);
        }
        fs::iput(*newInode);

        if (!ext2::AddEntryToDirectory(*parent, inum, EXT2_FT_SYMLINK, component)) {
            // TODO deallocate inode
            return std::unexpected(error::Code::OutOfSpace);
        }
        fs::idirty(*parent);
        fs::iput(*parent);
        return 0;
    }

    std::expected<int, error::Code> MakeDirectory(const char* path, int mode)
    {
        Inode* parent;
        char component[MaxPathLength];
        if (auto inode = namei2(path, true, parent, component); inode != nullptr) {
            fs::iput(*parent);
            fs::iput(*inode);
            return std::unexpected(error::Code::AlreadyExists);
        }
        if (parent == nullptr)
            return std::unexpected(error::Code::NoEntry);

        auto r = ext2::CreateDirectory(*parent, component, mode);
        fs::iput(*parent);
        return -r;
    }

    std::expected<int, error::Code> RemoveDirectory(const char* path)
    {
        Inode* parent;
        char component[MaxPathLength];
        Inode* inode = namei2(path, true, parent, component);
        if (inode == nullptr) {
            if (parent != nullptr)
                fs::iput(*parent);
            return std::unexpected(error::Code::NoEntry);
        }

        if ((inode->ext2inode->i_mode & EXT2_S_IFMASK) != EXT2_S_IFDIR) {
            fs::iput(*parent);
            fs::iput(*inode);
            return std::unexpected(error::Code::NotADirectory);
        }

        if (!IsDirectoryEmpty(*inode)) {
            fs::iput(*parent);
            fs::iput(*inode);
            return std::unexpected(error::Code::NotEmpty);
        }

        if (!ext2::RemoveEntryFromDirectory(*parent, component)) {
            fs::iput(*parent);
            fs::iput(*inode);
            return std::unexpected(error::Code::IOError);
        }
        --parent->ext2inode->i_links_count;
        fs::idirty(*parent);
        fs::iput(*parent);
        return ext2::RemoveDirectory(*inode);
    }

    std::expected<int, error::Code> ResolveDirectoryName(Inode& inode, char* buffer, int bufferSize)
    {
        if (inode.ext2inode == nullptr || (inode.ext2inode->i_mode & EXT2_S_IFDIR) == 0)
            return std::unexpected(error::Code::NotADirectory);
        if (bufferSize < 2)
            return std::unexpected(error::Code::NameTooLong);
        Inode* current = &inode;
        iref(*current);

        int currentPosition = bufferSize - 1;
        buffer[currentPosition] = '\0';
        while (current != rootInode) {
            Inode* parent = LookupInDirectory(*current, "..");
            if (parent == nullptr)
                break;

            // Find the current inode's name
            fs::DEntry dentry;
            if (!LookupInodeByNumber(*parent, current->inum, dentry)) {
                fs::iput(*parent);
                fs::iput(*current);
                return std::unexpected(error::Code::NoEntry);
            }

            const int entryLength = strlen(dentry.d_name);
            if (currentPosition - entryLength <= 0) {
                fs::iput(*parent);
                fs::iput(*current);
                return std::unexpected(error::Code::NameTooLong);
            }
            memcpy(buffer + currentPosition - entryLength, dentry.d_name, entryLength);
            buffer[currentPosition - entryLength - 1] = '/';
            currentPosition -= entryLength + 1;

            fs::iput(*current);
            current = parent;
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

    bool Stat(Inode& inode, stat& sbuf)
    {
        if (inode.ext2inode == nullptr)
            return false;
        const auto& e2i = *inode.ext2inode;
        sbuf.st_dev = inode.dev;
        sbuf.st_ino = inode.inum;
        sbuf.st_mode = e2i.i_mode;
        sbuf.st_uid = e2i.i_uid;
        sbuf.st_size = e2i.i_size;
        sbuf.st_atime = e2i.i_atime;
        sbuf.st_ctime = e2i.i_ctime;
        sbuf.st_mtime = e2i.i_mtime;
        sbuf.st_gid = e2i.i_gid;
        sbuf.st_nlink = e2i.i_links_count;
        sbuf.st_blocks = e2i.i_blocks;
        return true;
    }

    std::expected<int, error::Code> Mknod(const char* path, mode_t mode, dev_t dev)
    {
        Inode* parent;
        char component[MaxPathLength];
        if (auto inode = namei2(path, true, parent, component); inode != nullptr) {
            fs::iput(*parent);
            fs::iput(*inode);
            return std::unexpected(error::Code::AlreadyExists);
        }
        if (parent == nullptr)
            return std::unexpected(error::Code::NoEntry);

        const auto type = mode & EXT2_S_IFMASK;
        if (type != EXT2_S_IFBLK && type != EXT2_S_IFCHR)
            return std::unexpected(error::Code::InvalidArgument);

        const auto result = ext2::CreateSpecial(*parent, component, mode, dev);
        fs::iput(*parent);
        if (result) {
            fs::iput(**result);
            return 0;
        }
        return result.error();
    }
} // namespace fs
