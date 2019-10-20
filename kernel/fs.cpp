#include "fs.h"
#include "bio.h"
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
            inline constexpr unsigned int NumberOfInodes = 20;
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

        ext2::ReadInode(dev, inum, *available->ext2inode);
        return available;
    }

    void iput(Inode& inode)
    {
        assert(inode.refcount > 0);
        if (--inode.refcount == 0) {
            // TODO: write dirty inode, etc
        }
    }

    void iref(Inode& inode)
    {
        assert(inode.refcount > 0);
        ++inode.refcount;
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

            // printf("{offset %d count %d chunkLen %d inodeblock %d}\n", offset,count, chunkLen,
            // offset / bio::BlockSize);
            const uint32_t blockNr = ext2::bmap(inode, offset / bio::BlockSize);
            auto& buf = bio::bread(inode.dev, blockNr);
            memcpy(d, buf.data + (offset % bio::BlockSize), chunkLen);
            bio::brelse(buf);

            d += chunkLen;
            offset += chunkLen;
            count -= chunkLen;
        }
        return d - reinterpret_cast<char*>(dst);
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
            if (strcmp(dentry.d_name, item) != 0)
                continue;
            return iget(inode.dev, dentry.d_ino);
        }
        return nullptr;
    }

    Inode* namei(const char* path)
    {
        auto current_inode = [&]() {
            if (path[0] == '/')
                return rootInode;
            else
                return process::GetCurrent().cwd;
        }();
        iref(*current_inode);

        char component[MaxPathLength];
        while (IsolatePathComponent(path, component)) {
            auto new_inode = LookupInDirectory(*current_inode, component);
            iput(*current_inode);
            if (new_inode == nullptr)
                return nullptr;
            current_inode = new_inode;
        }

        return current_inode;
    }

    bool Stat(Inode& inode, stat& sbuf)
    {
        if(inode.ext2inode == nullptr) return false;
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
