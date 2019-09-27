#include "ext2.h"
#include "bio.h"
#include "fs.h"
#include "lib.h"

namespace ext2
{
    namespace
    {
        ext2::Superblock superblock;
        unsigned int blockSize;
        unsigned int biosPerBlock;

        constexpr inline fs::InodeNumber rootInodeNumber = 2;

        template<typename Buffer>
        void ReadBlocks(fs::Device dev, bio::BlockNumber blockNr, unsigned int count, Buffer* dest)
        {
            for (unsigned int n = 0; n < count; ++n) {
                auto& buf = bio::bread(dev, blockNr + n);
                memcpy(
                    reinterpret_cast<char*>(dest) + n * bio::BlockSize, buf.data, bio::BlockSize);
                bio::brelse(buf);
            }
        }

        void ReadBlockGroup(fs::Device dev, const int bgNumber, BlockGroup& blockGroup)
        {
            const auto blockNr = [&]() {
                bio::BlockNumber blockNr = 1 + (bgNumber * sizeof(BlockGroup)) / blockSize;
                blockNr += superblock.s_first_data_block;
                blockNr *= biosPerBlock;
                blockNr += ((bgNumber * sizeof(BlockGroup)) % blockSize) / bio::BlockSize;
                return blockNr;
            }();
            auto& buf = bio::bread(dev, blockNr);
            memcpy(
                reinterpret_cast<void*>(&blockGroup),
                buf.data + (bgNumber * sizeof(BlockGroup) % bio::BlockSize), sizeof(BlockGroup));
        }

    } // namespace

    void ReadInode(fs::Device dev, fs::InodeNumber inum, Inode& inode)
    {
        inum--; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;
        const uint32_t iindex = inum % superblock.s_inodes_per_group;

        BlockGroup blockGroup;
        ReadBlockGroup(dev, bgroup, blockGroup);
        /*
                printf(
                    "blockgroup %d => bitmap %d inode %d table %d free blocks %d free inodes %d used
           dirs "
                    "%d\n",
                    bgroup, blockGroup.bg_block_bitmap, blockGroup.bg_inode_bitmap,
                    blockGroup.bg_inode_table, blockGroup.bg_free_blocks_count,
                    blockGroup.bg_free_inodes_count, blockGroup.bg_used_dirs_count);
        */

        const auto inodeBlockNr = [&]() {
            bio::BlockNumber blockNr = blockGroup.bg_inode_table;
            blockNr += (iindex * superblock.s_inode_size) / blockSize;
            blockNr *= biosPerBlock;
            blockNr += ((iindex * superblock.s_inode_size) % blockSize) / bio::BlockSize;
            return blockNr;
        }();

        auto& buf = bio::bread(dev, inodeBlockNr);
        unsigned int idx = (iindex * superblock.s_inode_size) % bio::BlockSize;
        const Inode& in = *reinterpret_cast<Inode*>(&buf.data[idx]);
        inode = in;
        bio::brelse(buf);
    }

    uint32_t DetermineIndirect(Inode& inode, unsigned int& inodeBlockNr, int& level)
    {
        auto pointersPerBlock = blockSize / sizeof(uint32_t);
        inodeBlockNr -= 12;
        if (inodeBlockNr < pointersPerBlock) {
            level = 0;
            return inode.i_block[12];
        }

        inodeBlockNr -= pointersPerBlock;
        if (inodeBlockNr < (pointersPerBlock * pointersPerBlock)) {
            level = 1;
            return inode.i_block[13];
        }

        inodeBlockNr -= pointersPerBlock * pointersPerBlock;
        if (inodeBlockNr < (pointersPerBlock * pointersPerBlock * (pointersPerBlock + 1))) {
            level = 2;
            return inode.i_block[14];
        }
        panic("fourth indirect");
    }

    uint32_t bmap(fs::Inode& inode, unsigned int inodeBlockNr)
    {
        uint32_t ext2BlockNr = inodeBlockNr / biosPerBlock;
        uint32_t bioBlockOffset = inodeBlockNr % biosPerBlock;
        if (ext2BlockNr < 12) {
            return (inode.ext2inode->i_block[ext2BlockNr] * biosPerBlock) + bioBlockOffset;
        }

        int level;
        auto indirect = DetermineIndirect(*inode.ext2inode, ext2BlockNr, level);
        int block_shift = superblock.s_log_block_size + 8;
        do {
            auto blockIndex =
                (ext2BlockNr >> (block_shift * level)) % (blockSize / sizeof(uint32_t));
            indirect *= biosPerBlock;
            while (blockIndex >= bio::BlockSize / sizeof(uint32_t)) {
                blockIndex -= bio::BlockSize / sizeof(uint32_t);
                indirect++;
            }
            auto& buf = bio::bread(inode.dev, indirect);
            indirect = [&]() {
                const auto blocks = reinterpret_cast<uint32_t*>(buf.data);
                return blocks[blockIndex];
            }();
            bio::brelse(buf);
        } while (--level >= 0);

        return indirect * biosPerBlock + bioBlockOffset;
    }

    bool ReadDirectory(fs::Inode& dirInode, off_t& offset, fs::DEntry& dentry)
    {
        union {
            DirectoryEntry de;
            char block[bio::BlockSize];
        } u;

        while (offset < dirInode.ext2inode->i_size) {
            int n = fs::Read(
                dirInode, reinterpret_cast<void*>(&u.block), offset,
                sizeof(DirectoryEntry) + fs::MaxDirectoryEntryNameLength);
            if (n <= 0)
                return false;

            const auto& de = u.de;
            if (de.name_len >= fs::MaxDirectoryEntryNameLength)
                continue;

            dentry.d_ino = de.inode;
            memcpy(dentry.d_name, reinterpret_cast<const char*>(de.name), de.name_len);
            dentry.d_name[de.name_len] = '\0';
            offset += de.rec_len;
            return true;
        }

        return false;
    }

    fs::Inode* Mount(fs::Device dev)
    {
        // Piece the superblock together
        ReadBlocks(dev, 2, sizeof(Superblock) / bio::BlockSize, &superblock);
        if (superblock.s_magic != constants::magic::Magic)
            return nullptr;
        blockSize = 1024L << superblock.s_log_block_size;
        biosPerBlock = blockSize / bio::BlockSize;
        return fs::iget(dev, rootInodeNumber);
    }

} // namespace ext2
