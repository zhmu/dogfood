#include "ext2.h"
#include "bio.h"
#include "fs.h"
#include "lib.h"
#include <dogfood/errno.h>

namespace ext2
{
    namespace
    {
        ext2::Superblock superblock;
        unsigned int blockSize;
        unsigned int biosPerBlock;
        unsigned int numberOfBlockGroups;

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

        template<typename Buffer>
        void WriteBlocks(
            fs::Device dev, bio::BlockNumber blockNr, unsigned int count, const Buffer* source)
        {
            for (unsigned int n = 0; n < count; ++n) {
                auto& buf = bio::bread(dev, blockNr + n);
                memcpy(
                    buf.data, reinterpret_cast<const char*>(source) + n * bio::BlockSize,
                    bio::BlockSize);
                bio::bwrite(buf);
                bio::brelse(buf);
            }
        }

        bio::BlockNumber CalculateBlockGroupBioBlockNumber(const int bgNumber)
        {
            bio::BlockNumber blockNr = 1 + (bgNumber * sizeof(BlockGroup)) / blockSize;
            blockNr += superblock.s_first_data_block;
            blockNr *= biosPerBlock;
            blockNr += ((bgNumber * sizeof(BlockGroup)) % blockSize) / bio::BlockSize;
            return blockNr;
        }

        void ReadBlockGroup(fs::Device dev, const int bgNumber, BlockGroup& blockGroup)
        {
            auto& buf = bio::bread(dev, CalculateBlockGroupBioBlockNumber(bgNumber));
            memcpy(
                reinterpret_cast<void*>(&blockGroup),
                buf.data + (bgNumber * sizeof(BlockGroup) % bio::BlockSize), sizeof(BlockGroup));
            bio::brelse(buf);
        }

        void WriteBlockGroup(fs::Device dev, const int bgNumber, BlockGroup& blockGroup)
        {
            auto& buf = bio::bread(dev, CalculateBlockGroupBioBlockNumber(bgNumber));
            memcpy(
                buf.data + (bgNumber * sizeof(BlockGroup) % bio::BlockSize),
                reinterpret_cast<void*>(&blockGroup), sizeof(BlockGroup));
            bio::bwrite(buf);
            bio::brelse(buf);
        }

        void UpdateSuperblock(fs::Device dev)
        {
            WriteBlocks(dev, 2, sizeof(Superblock) / bio::BlockSize, &superblock);
        }

    } // namespace

    void ReadInode(fs::Device dev, fs::InodeNumber inum, Inode& inode)
    {
        inum--; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;
        const uint32_t iindex = inum % superblock.s_inodes_per_group;

        BlockGroup blockGroup;
        ReadBlockGroup(dev, bgroup, blockGroup);

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

    template<typename Function>
    void UpdateInodeBlockGroup(const fs::Device dev, fs::InodeNumber inum, Function f)
    {
        inum--; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;

        BlockGroup blockGroup;
        ReadBlockGroup(dev, bgroup, blockGroup);
        f(blockGroup);
        WriteBlockGroup(dev, bgroup, blockGroup);
    }

    void WriteInode(fs::Inode& inode)
    {
        auto inum = inode.inum - 1; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;
        const uint32_t iindex = inum % superblock.s_inodes_per_group;

        BlockGroup blockGroup;
        ReadBlockGroup(inode.dev, bgroup, blockGroup);

        const auto inodeBlockNr = [&]() {
            bio::BlockNumber blockNr = blockGroup.bg_inode_table;
            blockNr += (iindex * superblock.s_inode_size) / blockSize;
            blockNr *= biosPerBlock;
            blockNr += ((iindex * superblock.s_inode_size) % blockSize) / bio::BlockSize;
            return blockNr;
        }();

        auto& buf = bio::bread(inode.dev, inodeBlockNr);
        unsigned int idx = (iindex * superblock.s_inode_size) % bio::BlockSize;
        Inode& in = *reinterpret_cast<Inode*>(&buf.data[idx]);
        in = *inode.ext2inode;
        bio::bwrite(buf);
        bio::brelse(buf);
    }

    template<typename Strategy>
    bool AllocateFromBitmap(
        const fs::Device dev, const uint32_t initialBlockGroup, uint32_t& inum, Strategy)
    {
        constexpr auto bitsPerBlock = bio::BlockSize * 8;
        uint32_t bgroup = initialBlockGroup;
        do {
            BlockGroup blockGroup;
            ReadBlockGroup(dev, bgroup, blockGroup);
            if (Strategy::HasFreeItems(blockGroup)) {
                const auto bitmapFirstBlockNr = Strategy::GetBitmapBlock(blockGroup) * biosPerBlock;
                for (int itemIndex = 0; itemIndex < Strategy::GetItemsPerGroup(); ++itemIndex) {
                    auto& buf = bio::bread(dev, bitmapFirstBlockNr + (itemIndex / bitsPerBlock));
                    auto& bitmapPtr = buf.data[(itemIndex % bitsPerBlock) / 8];
                    auto bitmapBit = (1 << (itemIndex % 8));
                    if ((bitmapPtr & bitmapBit) == 0) {
                        inum = (bgroup * Strategy::GetItemsPerGroup()) + itemIndex;
                        bitmapPtr |= bitmapBit;
                        bio::bwrite(buf);
                        bio::brelse(buf);

                        Strategy::DecrementFreeItemCount(blockGroup);
                        WriteBlockGroup(dev, bgroup, blockGroup);
                        return true;
                    }
                    bio::brelse(buf);
                }
            }
            bgroup = (bgroup + 1) % numberOfBlockGroups;
        } while (bgroup != initialBlockGroup);

        return false;
    }

    template<typename Strategy>
    bool
    FreeFromBitmap(const fs::Device dev, const uint32_t bgroup, const uint32_t itemIndex, Strategy)
    {
        constexpr auto bitsPerBlock = bio::BlockSize * 8;
        BlockGroup blockGroup;
        ReadBlockGroup(dev, bgroup, blockGroup);

        const auto bitmapFirstBlockNr = Strategy::GetBitmapBlock(blockGroup) * biosPerBlock;
        auto& buf = bio::bread(dev, bitmapFirstBlockNr + (itemIndex / bitsPerBlock));
        auto& bitmapPtr = buf.data[(itemIndex % bitsPerBlock) / 8];
        auto bitmapBit = (1 << (itemIndex % 8));
        if ((bitmapPtr & bitmapBit) == 0) {
            bio::brelse(buf);
            return false;
        }
        bitmapPtr &= ~bitmapBit;
        bio::bwrite(buf);
        bio::brelse(buf);

        Strategy::IncrementFreeItemCount(blockGroup);
        WriteBlockGroup(dev, bgroup, blockGroup);
        return true;
    }

    struct InodeStrategy {
        static uint32_t GetBitmapBlock(const BlockGroup& bg) { return bg.bg_inode_bitmap; }
        static uint32_t GetItemsPerGroup() { return superblock.s_inodes_per_group; }
        static bool HasFreeItems(const BlockGroup& bg) { return bg.bg_free_inodes_count > 0; }
        static void DecrementFreeItemCount(BlockGroup& bg) { --bg.bg_free_inodes_count; }
        static void IncrementFreeItemCount(BlockGroup& bg) { ++bg.bg_free_inodes_count; }
    };

    struct BlockStrategy {
        static uint32_t GetBitmapBlock(const BlockGroup& bg) { return bg.bg_block_bitmap; }
        static uint32_t GetItemsPerGroup() { return superblock.s_blocks_per_group; }
        static bool HasFreeItems(const BlockGroup& bg) { return bg.bg_free_blocks_count > 0; }
        static void DecrementFreeItemCount(BlockGroup& bg) { --bg.bg_free_blocks_count; }
        static void IncrementFreeItemCount(BlockGroup& bg) { ++bg.bg_free_blocks_count; }
    };

    uint32_t AllocateInode(fs::Inode& dirInode)
    {
        const auto initialBlockGroup = (dirInode.inum - 1) / superblock.s_inodes_per_group;
        uint32_t inum;
        if (!AllocateFromBitmap(dirInode.dev, initialBlockGroup, inum, InodeStrategy{}))
            return 0;
        --superblock.s_free_inodes_count;
        UpdateSuperblock(dirInode.dev);
        return inum + 1;
    }

    bool AllocateBlock(fs::Inode& inode, uint32_t& blockNum)
    {
        // initialBlockGroup is wrong; we should consider the last block group used
        const auto initialBlockGroup = (inode.inum - 1) / superblock.s_inodes_per_group;
        if (AllocateFromBitmap(inode.dev, initialBlockGroup, blockNum, BlockStrategy{})) {
            --superblock.s_free_blocks_count;
            UpdateSuperblock(inode.dev);
            return true;
        } else
            return false;
    }

    bool FreeDataBlock(const fs::Device dev, uint32_t blockNr)
    {
        const auto bgroup = blockNr / superblock.s_blocks_per_group;
        const auto index = blockNr % superblock.s_blocks_per_group;
        if (!FreeFromBitmap(dev, bgroup, index, BlockStrategy{}))
            return false;
        ++superblock.s_free_blocks_count;
        UpdateSuperblock(dev);
        return true;
    }

    template<typename Func>
    void TraverseBlockPointers(const fs::Device dev, const uint32_t blockNr, Func f)
    {
        const auto pointersPerBlock = blockSize / sizeof(uint32_t);
        const auto pointersPerBioBlock = bio::BlockSize / sizeof(uint32_t);
        for (int n = 0; n < pointersPerBlock; ++n) {
            const auto bioBlockNr = blockNr * biosPerBlock + (n / pointersPerBioBlock);
            const auto offset = (n % pointersPerBioBlock) * sizeof(uint32_t);
            auto& bio = bio::bread(dev, bioBlockNr);
            const auto indirectBlock = *reinterpret_cast<uint32_t*>(bio.data + offset);
            f(indirectBlock);
            bio::brelse(bio);
        }
    }

    void FreeDataBlocks(fs::Inode& inode)
    {
        auto freeBlockIfInUse = [&](const uint32_t blockNr) {
            if (blockNr == 0)
                return;
            FreeDataBlock(inode.dev, blockNr);
        };

        auto blocks = &inode.ext2inode->i_block[0];
        for (int n = 0; n < 12; ++n) {
            freeBlockIfInUse(blocks[n]);
        }

        if (blocks[12] != 0) {
            TraverseBlockPointers(
                inode.dev, blocks[12], [&](const auto block) { freeBlockIfInUse(block); });
            freeBlockIfInUse(blocks[12]);
        }
        if (blocks[13] != 0) {
            TraverseBlockPointers(inode.dev, blocks[13], [&](const auto indirectBlockNr) {
                if (indirectBlockNr == 0)
                    return;
                TraverseBlockPointers(
                    inode.dev, indirectBlockNr, [&](const auto block) { freeBlockIfInUse(block); });
                freeBlockIfInUse(indirectBlockNr);
            });
            freeBlockIfInUse(blocks[13]);
        }
        if (blocks[14] != 0) {
            TraverseBlockPointers(inode.dev, blocks[14], [&](const auto firstIndirectBlockNr) {
                if (firstIndirectBlockNr == 0)
                    return;
                TraverseBlockPointers(
                    inode.dev, firstIndirectBlockNr, [&](const auto secondIndirectBlockNumber) {
                        if (secondIndirectBlockNumber == 0)
                            return;
                        TraverseBlockPointers(
                            inode.dev, secondIndirectBlockNumber,
                            [&](const auto block) { freeBlockIfInUse(block); });
                        freeBlockIfInUse(secondIndirectBlockNumber);
                    });
                freeBlockIfInUse(firstIndirectBlockNr);
            });
            freeBlockIfInUse(blocks[14]);
        }
    }

    void FreeInode(fs::Inode& inode)
    {
        const auto bgroup = (inode.inum - 1) / superblock.s_inodes_per_group;
        const auto index = (inode.inum - 1) % superblock.s_inodes_per_group;
        if (!FreeFromBitmap(inode.dev, bgroup, index, InodeStrategy{}))
            return;

        ++superblock.s_free_inodes_count;
        UpdateSuperblock(inode.dev);

        memset(inode.ext2inode, 0, sizeof(Inode));
        fs::idirty(inode);
        fs::iput(inode);
    }

    uint32_t* DetermineIndirect(Inode& inode, unsigned int& inodeBlockNr, int& level)
    {
        const auto pointersPerBlock = blockSize / sizeof(uint32_t);
        inodeBlockNr -= 12;
        if (inodeBlockNr < pointersPerBlock) {
            level = 0;
            return &inode.i_block[12];
        }

        inodeBlockNr -= pointersPerBlock;
        if (inodeBlockNr < (pointersPerBlock * pointersPerBlock)) {
            level = 1;
            return &inode.i_block[13];
        }

        inodeBlockNr -= pointersPerBlock * pointersPerBlock;
        if (inodeBlockNr < (pointersPerBlock * pointersPerBlock * (pointersPerBlock + 1))) {
            level = 2;
            return &inode.i_block[14];
        }
        panic("fourth indirect");
    }

    bool AllocateNewBlockAsNecessary(
        fs::Inode& inode, uint32_t* block, bio::Buffer* bio, const bool createIfNecessary)
    {
        if (!createIfNecessary)
            return *block != 0;
        if (*block != 0)
            return true;

        uint32_t newBlock;
        if (!AllocateBlock(inode, newBlock))
            return false;

        *block = newBlock;
        ++inode.ext2inode->i_blocks;
        fs::idirty(inode);
        if (bio != nullptr)
            bio::bwrite(*bio);

        // Zero new block content
        for (int n = 0; n < biosPerBlock; ++n) {
            auto& newBIO = bio::bread(inode.dev, (newBlock * biosPerBlock) + n);
            memset(newBIO.data, 0, bio::BlockSize);
            bio::bwrite(newBIO);
            bio::brelse(newBIO);
        }
        return true;
    }

    uint32_t bmap(fs::Inode& inode, unsigned int inodeBlockNr, const bool createIfNecessary)
    {
        uint32_t ext2BlockNr = inodeBlockNr / biosPerBlock;
        uint32_t bioBlockOffset = inodeBlockNr % biosPerBlock;
        if (ext2BlockNr < 12) {
            auto block = &inode.ext2inode->i_block[ext2BlockNr];
            if (!AllocateNewBlockAsNecessary(inode, block, nullptr, createIfNecessary))
                return 0;
            return (*block * biosPerBlock) + bioBlockOffset;
        }

        int level;
        auto indirectPtr = DetermineIndirect(*inode.ext2inode, ext2BlockNr, level);
        if (!AllocateNewBlockAsNecessary(inode, indirectPtr, nullptr, createIfNecessary))
            return 0;
        auto indirect = *indirectPtr;
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
            indirect = [&]() -> uint32_t {
                const auto blocks = reinterpret_cast<uint32_t*>(buf.data);
                auto blockPtr = &blocks[blockIndex];
                if (!AllocateNewBlockAsNecessary(inode, blockPtr, &buf, createIfNecessary))
                    return 0;
                return *blockPtr;
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
            if (de.name_len >= fs::MaxDirectoryEntryNameLength) {
                offset += de.rec_len;
                continue;
            }
            if (de.inode == 0) {
                offset += de.rec_len;
                continue;
            }

            dentry.d_ino = de.inode;
            memcpy(dentry.d_name, reinterpret_cast<const char*>(de.name), de.name_len);
            dentry.d_name[de.name_len] = '\0';
            offset += de.rec_len;
            return true;
        }

        return false;
    }

    template<typename T>
    constexpr T RoundUpToMultipleOf4(T value)
    {
        if (value % 4)
            value += 4 - (value % 4);
        return value;
    }

    bool WriteDirectoryEntry(
        fs::Inode& dirInode, off_t offset, fs::InodeNumber inum, uint16_t newEntryRecordLength,
        int type, const char* name)
    {
        union {
            DirectoryEntry de;
            char block[bio::BlockSize];
        } newEntry;
        newEntry.de.inode = inum;
        newEntry.de.rec_len = newEntryRecordLength;
        newEntry.de.name_len = strlen(name);
        newEntry.de.file_type = type;
        memcpy(newEntry.de.name, name, newEntry.de.name_len);
        const auto entryLength = sizeof(DirectoryEntry) + newEntry.de.name_len;
        return fs::Write(dirInode, reinterpret_cast<void*>(&newEntry), offset, entryLength) ==
               entryLength;
    }

    bool AddEntryToDirectory(fs::Inode& dirInode, fs::InodeNumber inum, int type, const char* name)
    {
        const auto newEntryLength = RoundUpToMultipleOf4(sizeof(DirectoryEntry) + strlen(name));
        off_t offset = 0;
        while (offset < dirInode.ext2inode->i_size) {
            DirectoryEntry dentry;
            int n = fs::Read(
                dirInode, reinterpret_cast<void*>(&dentry), offset, sizeof(DirectoryEntry));
            if (n <= 0)
                break;

            const auto currentEntryLength =
                dentry.inode != 0
                    ? RoundUpToMultipleOf4(sizeof(struct DirectoryEntry) + dentry.name_len)
                    : 0;
            if (dentry.rec_len - currentEntryLength < newEntryLength) {
                offset += dentry.rec_len;
                continue;
            }

            // TODO check if this would exceeds a block boundary and break if it does

            // Update record length - if we have a record
            const auto newEntryRecordLength = dentry.rec_len - currentEntryLength;
            if (currentEntryLength > 0) {
                dentry.rec_len = currentEntryLength;
                if (fs::Write(
                        dirInode, reinterpret_cast<void*>(&dentry), offset,
                        sizeof(DirectoryEntry)) != sizeof(DirectoryEntry))
                    return false;
                offset += dentry.rec_len;
            }

            if (!WriteDirectoryEntry(dirInode, offset, inum, newEntryRecordLength, type, name))
                return false; // TODO undo previous changes
            return true;
        }

        const auto newEntryRecordLength = blockSize;
        return WriteDirectoryEntry(dirInode, offset, inum, newEntryRecordLength, type, name);
    }

    bool RemoveEntryFromDirectory(fs::Inode& dirInode, const char* name)
    {
        const auto nameLength = strlen(name);
        off_t offset = 0;
        off_t previousOffset = 0;
        DirectoryEntry previousEntry{};
        while (offset < dirInode.ext2inode->i_size) {
            DirectoryEntry dentry;
            char component[fs::MaxPathLength];
            if (fs::Read(
                    dirInode, reinterpret_cast<void*>(&dentry), offset, sizeof(DirectoryEntry)) !=
                sizeof(DirectoryEntry))
                return false;
            if (fs::Read(
                    dirInode, reinterpret_cast<void*>(component), offset + sizeof(DirectoryEntry),
                    dentry.name_len) != dentry.name_len) {
                return false;
            }
            if (dentry.name_len != nameLength || memcmp(component, name, nameLength) != 0) {
                previousOffset = offset;
                previousEntry = dentry;
                offset += dentry.rec_len;
                continue;
            }

            if (previousEntry.rec_len > 0) {
                previousEntry.rec_len += dentry.rec_len;
                return fs::Write(
                           dirInode, reinterpret_cast<void*>(&previousEntry), previousOffset,
                           sizeof(DirectoryEntry)) == sizeof(DirectoryEntry);
            }

            dentry.inode = 0;
            return fs::Write(
                       dirInode, reinterpret_cast<void*>(&dentry), offset,
                       sizeof(DirectoryEntry)) == sizeof(DirectoryEntry);
        }
        return false;
    }

    int CreateDirectory(fs::Inode& parent, const char* name, int mode)
    {
        auto inum = ext2::AllocateInode(parent);
        if (inum == 0)
            return ENOSPC;

        auto newInode = fs::iget(parent.dev, inum);
        assert(newInode != nullptr);
        {
            auto& e2i = *newInode->ext2inode;
            memset(&e2i, 0, sizeof(e2i));
            e2i.i_mode = EXT2_S_IFDIR | mode;
            e2i.i_links_count = 2; // '.' and entry in parent
        }

        if (!ext2::AddEntryToDirectory(parent, inum, EXT2_FT_DIR, name)) {
            // TODO deallocate inode
            return ENOSPC;
        }

        {
            ext2::DirectoryEntry dirEntry;
            memset(&dirEntry, 0, sizeof(dirEntry));
            dirEntry.rec_len = blockSize;
            if (fs::Write(*newInode, &dirEntry, 0, sizeof(dirEntry)) != sizeof(dirEntry))
                return ENOSPC; // TODO undo damage
            newInode->ext2inode->i_size = blockSize;
        }

        if (!ext2::AddEntryToDirectory(*newInode, inum, EXT2_FT_DIR, ".")) {
            // TODO deallocate inode
            return ENOSPC;
        }
        if (!ext2::AddEntryToDirectory(*newInode, parent.inum, EXT2_FT_DIR, "..")) {
            // TODO deallocate inode
            return ENOSPC;
        }
        ++parent.ext2inode->i_links_count;
        fs::idirty(parent);

        UpdateInodeBlockGroup(
            newInode->dev, newInode->inum, [](auto& bg) { ++bg.bg_used_dirs_count; });

        fs::idirty(*newInode);
        fs::iput(*newInode);
        return 0;
    }

    void Unlink(fs::Inode& inode)
    {
        if (--inode.ext2inode->i_links_count > 0) {
            fs::idirty(inode);
            fs::iput(inode);
            return;
        }

        FreeDataBlocks(inode);
        FreeInode(inode);
    }

    int RemoveDirectory(fs::Inode& inode)
    {
        if (!ext2::RemoveEntryFromDirectory(inode, ".."))
            return EIO;
        if (!ext2::RemoveEntryFromDirectory(inode, "."))
            return EIO;
        UpdateInodeBlockGroup(inode.dev, inode.inum, [](auto& bg) { --bg.bg_used_dirs_count; });

        FreeDataBlocks(inode);
        FreeInode(inode);
        return 0;
    }

    fs::Inode* Mount(fs::Device dev)
    {
        // Piece the superblock together
        ReadBlocks(dev, 2, sizeof(Superblock) / bio::BlockSize, &superblock);
        if (superblock.s_magic != constants::magic::Magic)
            return nullptr;
        blockSize = 1024L << superblock.s_log_block_size;
        biosPerBlock = blockSize / bio::BlockSize;
        numberOfBlockGroups = (superblock.s_blocks_count - superblock.s_first_data_block) /
                              superblock.s_blocks_per_group;
        return fs::iget(dev, rootInodeNumber);
    }
} // namespace ext2
