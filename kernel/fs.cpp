#include "fs.h"
#include "bio.h"
#include "errno.h"
#include "ext2.h"
#include "process.h"
#include "stat.h"
#include "lib.h"

namespace fs
{
    namespace
    {
        constexpr inline Device rootDeviceNumber = 0;

        namespace cache
        {
            inline constexpr unsigned int NumberOfInodes = 40;
            Inode inode[NumberOfInodes];
            ext2::Inode ext2inode[NumberOfInodes];
        } // namespace cache

        Inode* rootInode = nullptr;
    } // namespace

    void Initialize()
    {
        for (size_t n = 0; n < cache::NumberOfInodes; ++n) {
            cache::inode[n].ext2inode = &cache::ext2inode[n];
        }

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
        //inode.dirty = true;
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
            auto& buf = bio::bread(inode.dev, blockNr);
            memcpy(d, buf.data + (offset % bio::BlockSize), chunkLen);
            bio::brelse(buf);

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
            auto& buf = bio::bread(inode.dev, blockNr);
            memcpy(buf.data + (offset % bio::BlockSize), s, chunkLen);
            bio::bwrite(buf);
            bio::brelse(buf);

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

    // Does namei() but yields parent inode if the final piece did not exist
    Inode* namei2(const char* path, Inode*& parent, char* component)
    {
        auto current_inode = [&]() {
            if (path[0] == '/')
                return rootInode;
            else
                return process::GetCurrent().cwd;
        }();
        iref(*current_inode);

        const char* last_path = path;
        parent = nullptr;
        while (IsolatePathComponent(path, component)) {
            auto new_inode = LookupInDirectory(*current_inode, component);
            if (new_inode == nullptr) {
                if (strchr(path, '/') == nullptr) {
                    parent = current_inode;
                    path = last_path;
                } else
                    iput(*current_inode); // not final piece
                return nullptr;
            }
            iput(*current_inode);
            current_inode = new_inode;
            last_path = path;
        }

        return current_inode;
    }

    Inode* namei(const char* path)
    {
        Inode* parent;
        char component[MaxPathLength];
        auto inode = namei2(path, parent, component);
        if (inode != nullptr) return inode;
        if (parent != nullptr) iput(*parent);
        return nullptr;
    }

    int Open(const char* path, int flags, int mode, Inode*& inode)
    {
        Inode* parent;
        char component[MaxPathLength];
        inode = namei2(path, parent, component);
        if (inode != nullptr) {
            if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) return EEXIST;
            return 0;
        }
        if (parent == nullptr) return ENOENT;
        if ((flags & O_CREAT) == 0) return ENOENT;

        auto inum = ext2::AllocateInode(*parent);
        if (inum == 0) return ENOSPC;

        auto newInode = fs::iget(parent->dev, inum);
        assert(newInode != nullptr);
        {
            auto e2i = *newInode->ext2inode;
            memset(&e2i, 0, sizeof(e2i));
            e2i.i_mode = EXT2_S_IFREG | mode;
            e2i.i_links_count = 1;
        }
        fs::idirty(*newInode);

        if (!ext2::AddEntryToDirectory(*parent, *newInode, component)) {
            // TODO deallocate inode
            return ENOSPC;
        }
        inode = newInode;
        return 0;
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

    int ResolveDirectoryName(Inode& inode, char* buffer, int bufferSize)
    {
        if (inode.ext2inode == nullptr || (inode.ext2inode->i_mode & EXT2_S_IFDIR) == 0)
            return ENOTDIR;
        if (bufferSize < 2)
            return ENAMETOOLONG;
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
                return ENOENT;
            }

            const int entryLength = strlen(dentry.d_name);
            if (currentPosition - entryLength <= 0) {
                fs::iput(*parent);
                fs::iput(*current);
                return ENAMETOOLONG;
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

} // namespace fs
