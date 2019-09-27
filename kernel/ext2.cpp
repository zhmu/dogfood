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

        constexpr inline int RootDevice = 0;
        constexpr inline fs::ino_t RootInode = 2;

        template<typename Buffer>
        void ReadBlocks(bio::BlockNumber blockNr, unsigned int count, Buffer* dest)
        {
            for (unsigned int n = 0; n < count; ++n) {
                auto& buf = bio::bread(RootDevice, blockNr + n);
                memcpy(
                    reinterpret_cast<char*>(dest) + n * bio::BlockSize, buf.data, bio::BlockSize);
                bio::brelse(buf);
            }
        }

        void ReadBlockGroup(const int bgNumber, BlockGroup& blockGroup)
        {
            const auto blockNr = [&]() {
                bio::BlockNumber blockNr = 1 + (bgNumber * sizeof(BlockGroup)) / blockSize;
                blockNr += superblock.s_first_data_block;
                printf("bgNumber %d => ext2 block %d\n", bgNumber, blockNr);
                blockNr *= biosPerBlock;
                blockNr += ((bgNumber * sizeof(BlockGroup)) % blockSize) / bio::BlockSize;
                return blockNr;
            }();
            printf("bgNumber %d => block %d\n", bgNumber, blockNr);
            auto& buf = bio::bread(RootDevice, blockNr);
            memcpy(
                reinterpret_cast<void*>(&blockGroup),
                buf.data + (bgNumber * sizeof(BlockGroup) % bio::BlockSize), sizeof(BlockGroup));
        }

    } // namespace

    void iread(fs::ino_t inum, Inode& inode)
    {
        inum--; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;
        const uint32_t iindex = inum % superblock.s_inodes_per_group;

        BlockGroup blockGroup;
        ReadBlockGroup(bgroup, blockGroup);
        printf(
            "blockgroup %d => bitmap %d inode %d table %d free blocks %d free inodes %d used dirs "
            "%d\n",
            bgroup, blockGroup.bg_block_bitmap, blockGroup.bg_inode_bitmap,
            blockGroup.bg_inode_table, blockGroup.bg_free_blocks_count,
            blockGroup.bg_free_inodes_count, blockGroup.bg_used_dirs_count);

        const auto inodeBlockNr = [&]() {
            bio::BlockNumber blockNr = blockGroup.bg_inode_table;
            blockNr += (iindex * superblock.s_inode_size) / blockSize;
            blockNr *= biosPerBlock;
            blockNr += ((iindex * superblock.s_inode_size) % blockSize) / bio::BlockSize;
            return blockNr;
        }();

        auto& buf = bio::bread(RootDevice, inodeBlockNr);
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

    uint32_t bmap(Inode& inode, unsigned int inodeBlockNr)
    {
        uint32_t ext2BlockNr = inodeBlockNr / biosPerBlock;
        uint32_t bioBlockOffset = inodeBlockNr % biosPerBlock;
        if (ext2BlockNr < 12) {
            return (inode.i_block[ext2BlockNr] * biosPerBlock) + bioBlockOffset;
        }

        int level;
        auto indirect = DetermineIndirect(inode, ext2BlockNr, level);
        int block_shift = superblock.s_log_block_size + 8;
        do {
            auto blockIndex =
                (ext2BlockNr >> (block_shift * level)) % (blockSize / sizeof(uint32_t));
            indirect *= biosPerBlock;
            while (blockIndex >= bio::BlockSize / sizeof(uint32_t)) {
                blockIndex -= bio::BlockSize / sizeof(uint32_t);
                indirect++;
            }
            auto& buf = bio::bread(RootDevice, indirect);
            indirect = [&]() {
                const auto blocks = reinterpret_cast<uint32_t*>(buf.data);
                return blocks[blockIndex];
            }();
            bio::brelse(buf);
        } while (--level >= 0);

        return indirect * biosPerBlock + bioBlockOffset;
    }

    void Mount()
    {
        // Piece the superblock together
        ReadBlocks(2, sizeof(Superblock) / bio::BlockSize, &superblock);
        if (superblock.s_magic != constants::magic::Magic)
            panic("bad ext2 magic");
        blockSize = 1024L << superblock.s_log_block_size;
        biosPerBlock = blockSize / bio::BlockSize;
        printf("%d\n", biosPerBlock);

        // iread(RootInode, rootInode);

        for (int n = 13; n < 14; n++) {
            Inode inode;

            iread(n, inode);
            printf(
                "inode %d: mode %d uid %d size %d gid %d #links %d blocks %d flags %x\n", n,
                inode.i_mode, inode.i_uid, inode.i_size, inode.i_gid, inode.i_links_count,
                inode.i_blocks, inode.i_flags);

#if 1
            for (int ibl = 0; ibl < inode.i_blocks; ibl++) {
                auto bl = bmap(inode, ibl);
                printf("%d ==> %d\n", ibl, bl);
            }
#endif
        }
    }

} // namespace ext2
