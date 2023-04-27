#include "ext2.h"
#include "bio.h"
#include "fs.h"
#include "lib.h"
#include <optional>
#include <dogfood/errno.h>

// XXX Disable this warning for now; x86 can handle the unaligned access just fine
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

namespace ext2
{
    namespace
    {
        ext2::on_disk::Superblock superblock;
        unsigned int blockSize;
        unsigned int biosPerBlock;
        unsigned int numberOfBlockGroups;

        template<typename Buffer>
        void ReadBlocks(fs::Device dev, bio::BlockNumber blockNr, unsigned int count, Buffer* dest)
        {
            for (unsigned int n = 0; n < count; ++n) {
                auto buf = bio::ReadBlock(dev, blockNr + n);
                assert(buf);
                memcpy(
                    reinterpret_cast<char*>(dest) + n * bio::BlockSize, buf->data, bio::BlockSize);
            }
        }

        template<typename Buffer>
        void WriteBlocks(
            fs::Device dev, bio::BlockNumber blockNr, unsigned int count, const Buffer* source)
        {
            for (unsigned int n = 0; n < count; ++n) {
                auto buf = bio::ReadBlock(dev, blockNr + n);
                assert(buf);
                memcpy(
                    buf->data, reinterpret_cast<const char*>(source) + n * bio::BlockSize,
                    bio::BlockSize);
                bio::WriteBlock(std::move(buf));
            }
        }

        bio::BlockNumber CalculateBlockGroupBioBlockNumber(const int bgNumber)
        {
            bio::BlockNumber blockNr = 1 + (bgNumber * sizeof(on_disk::BlockGroup)) / blockSize;
            blockNr += superblock.s_first_data_block;
            blockNr *= biosPerBlock;
            blockNr += ((bgNumber * sizeof(on_disk::BlockGroup)) % blockSize) / bio::BlockSize;
            return blockNr;
        }

        void ReadBlockGroup(fs::Device dev, const int bgNumber, on_disk::BlockGroup& blockGroup)
        {
            auto buf = bio::ReadBlock(dev, CalculateBlockGroupBioBlockNumber(bgNumber));
            assert(buf);
            memcpy(
                reinterpret_cast<void*>(&blockGroup),
                buf->data + (bgNumber * sizeof(on_disk::BlockGroup) % bio::BlockSize), sizeof(on_disk::BlockGroup));
        }

        void WriteBlockGroup(fs::Device dev, const int bgNumber, on_disk::BlockGroup& blockGroup)
        {
            auto buf = bio::ReadBlock(dev, CalculateBlockGroupBioBlockNumber(bgNumber));
            assert(buf);
            memcpy(
                buf->data + (bgNumber * sizeof(on_disk::BlockGroup) % bio::BlockSize),
                reinterpret_cast<void*>(&blockGroup), sizeof(on_disk::BlockGroup));
            bio::WriteBlock(std::move(buf));
        }

        void UpdateSuperblock(fs::Device dev)
        {
            WriteBlocks(dev, 2, sizeof(on_disk::Superblock) / bio::BlockSize, &superblock);
        }

        result::MaybeInt ReadExact(fs::Inode& inode, void* dst, off_t offset, unsigned int count)
        {
            auto result = fs::Read(inode, dst, offset, count);
            if (!result) return result;
            if (*result != count) return result::Error(error::Code::IOError);
            return 0;
        }

        result::MaybeInt WriteExact(fs::Inode& inode, const void* dst, off_t offset, unsigned int count)
        {
            auto result = fs::Write(inode, dst, offset, count);
            if (!result) return result;
            if (*result != count) return result::Error(error::Code::IOError);
            return 0;
        }
    } // namespace

    result::MaybeInt AddEntryToDirectory(fs::Inode& dirInode, fs::InodeNumber inum, int type, const char* name);

    void ReadInode(fs::Device dev, fs::InodeNumber inum, on_disk::Inode& inode)
    {
        inum--; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;
        const uint32_t iindex = inum % superblock.s_inodes_per_group;

        on_disk::BlockGroup blockGroup;
        ReadBlockGroup(dev, bgroup, blockGroup);

        const auto inodeBlockNr = [&]() {
            bio::BlockNumber blockNr = blockGroup.bg_inode_table;
            blockNr += (iindex * superblock.s_inode_size) / blockSize;
            blockNr *= biosPerBlock;
            blockNr += ((iindex * superblock.s_inode_size) % blockSize) / bio::BlockSize;
            return blockNr;
        }();

        auto buf = bio::ReadBlock(dev, inodeBlockNr);
        assert(buf);
        unsigned int idx = (iindex * superblock.s_inode_size) % bio::BlockSize;
        const auto& in = *reinterpret_cast<on_disk::Inode*>(&buf->data[idx]);
        inode = in;
    }

    template<typename Function>
    void UpdateInodeBlockGroup(const fs::Device dev, fs::InodeNumber inum, Function f)
    {
        inum--; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;

        on_disk::BlockGroup blockGroup;
        ReadBlockGroup(dev, bgroup, blockGroup);
        f(blockGroup);
        WriteBlockGroup(dev, bgroup, blockGroup);
    }

    void WriteInode(fs::Inode& inode)
    {
        auto inum = inode.inum - 1; // inode 0 does not exist and is not stored

        const uint32_t bgroup = inum / superblock.s_inodes_per_group;
        const uint32_t iindex = inum % superblock.s_inodes_per_group;

        on_disk::BlockGroup blockGroup;
        ReadBlockGroup(inode.dev, bgroup, blockGroup);

        const auto inodeBlockNr = [&]() {
            bio::BlockNumber blockNr = blockGroup.bg_inode_table;
            blockNr += (iindex * superblock.s_inode_size) / blockSize;
            blockNr *= biosPerBlock;
            blockNr += ((iindex * superblock.s_inode_size) % blockSize) / bio::BlockSize;
            return blockNr;
        }();

        auto buf = bio::ReadBlock(inode.dev, inodeBlockNr);
        assert(buf);
        unsigned int idx = (iindex * superblock.s_inode_size) % bio::BlockSize;
        auto& in = *reinterpret_cast<on_disk::Inode*>(&buf->data[idx]);
        in = *inode.ext2inode;
        bio::WriteBlock(std::move(buf));
    }

    template<typename Strategy>
    bool AllocateFromBitmap(
        const fs::Device dev, const uint32_t initialBlockGroup, uint32_t& inum, Strategy)
    {
        constexpr auto bitsPerBlock = bio::BlockSize * 8;
        uint32_t bgroup = initialBlockGroup;
        do {
            on_disk::BlockGroup blockGroup;
            ReadBlockGroup(dev, bgroup, blockGroup);
            if (Strategy::HasFreeItems(blockGroup)) {
                const auto bitmapFirstBlockNr = Strategy::GetBitmapBlock(blockGroup) * biosPerBlock;
                for (int itemIndex = 0; itemIndex < Strategy::GetItemsPerGroup(); ++itemIndex) {
                    auto buf = bio::ReadBlock(dev, bitmapFirstBlockNr + (itemIndex / bitsPerBlock));
                    assert(buf);
                    auto& bitmapPtr = buf->data[(itemIndex % bitsPerBlock) / 8];
                    auto bitmapBit = (1 << (itemIndex % 8));
                    if ((bitmapPtr & bitmapBit) == 0) {
                        inum = (bgroup * Strategy::GetItemsPerGroup()) + itemIndex;
                        bitmapPtr |= bitmapBit;
                        bio::WriteBlock(std::move(buf));

                        Strategy::DecrementFreeItemCount(blockGroup);
                        WriteBlockGroup(dev, bgroup, blockGroup);
                        return true;
                    }
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
        on_disk::BlockGroup blockGroup;
        ReadBlockGroup(dev, bgroup, blockGroup);

        const auto bitmapFirstBlockNr = Strategy::GetBitmapBlock(blockGroup) * biosPerBlock;
        auto buf = bio::ReadBlock(dev, bitmapFirstBlockNr + (itemIndex / bitsPerBlock));
        assert(buf);
        auto& bitmapPtr = buf->data[(itemIndex % bitsPerBlock) / 8];
        auto bitmapBit = (1 << (itemIndex % 8));
        if ((bitmapPtr & bitmapBit) == 0) {
            return false;
        }
        bitmapPtr &= ~bitmapBit;
        bio::WriteBlock(std::move(buf));

        Strategy::IncrementFreeItemCount(blockGroup);
        WriteBlockGroup(dev, bgroup, blockGroup);
        return true;
    }

    struct InodeStrategy {
        static uint32_t GetBitmapBlock(const on_disk::BlockGroup& bg) { return bg.bg_inode_bitmap; }
        static uint32_t GetItemsPerGroup() { return superblock.s_inodes_per_group; }
        static bool HasFreeItems(const on_disk::BlockGroup& bg) { return bg.bg_free_inodes_count > 0; }
        static void DecrementFreeItemCount(on_disk::BlockGroup& bg) { --bg.bg_free_inodes_count; }
        static void IncrementFreeItemCount(on_disk::BlockGroup& bg) { ++bg.bg_free_inodes_count; }
    };

    struct BlockStrategy {
        static uint32_t GetBitmapBlock(const on_disk::BlockGroup& bg) { return bg.bg_block_bitmap; }
        static uint32_t GetItemsPerGroup() { return superblock.s_blocks_per_group; }
        static bool HasFreeItems(const on_disk::BlockGroup& bg) { return bg.bg_free_blocks_count > 0; }
        static void DecrementFreeItemCount(on_disk::BlockGroup& bg) { --bg.bg_free_blocks_count; }
        static void IncrementFreeItemCount(on_disk::BlockGroup& bg) { ++bg.bg_free_blocks_count; }
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

    result::Maybe<fs::InodeRef> CreateDirectoryEntry(fs::Inode& parent, uint16_t mode, int ft, const char* name)
    {
        auto inum = AllocateInode(parent);
        if (inum == 0)
            return result::Error(error::Code::OutOfSpace);

        auto result = fs::iget(parent.dev, inum);
        if (!result)
            return result::Error(result.error());
        auto newInode = std::move(*result);
        *newInode->ext2inode = { .i_mode = mode, .i_links_count = 1 };
        fs::idirty(*newInode);

        if (!AddEntryToDirectory(parent, inum, ft, name)) {
            // TODO deallocate inode
            return result::Error(error::Code::OutOfSpace);
        }
        return newInode;
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
            auto bio = bio::ReadBlock(dev, bioBlockNr);
            assert(bio);
            const auto indirectBlock = *reinterpret_cast<uint32_t*>(bio->data + offset);
            f(indirectBlock);
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

    void FreeInode(fs::InodeRef inode)
    {
        const auto bgroup = (inode->inum - 1) / superblock.s_inodes_per_group;
        const auto index = (inode->inum - 1) % superblock.s_inodes_per_group;
        if (!FreeFromBitmap(inode->dev, bgroup, index, InodeStrategy{}))
            return;

        ++superblock.s_free_inodes_count;
        UpdateSuperblock(inode->dev);

        memset(inode->ext2inode, 0, sizeof(on_disk::Inode));
        fs::idirty(*inode);
    }

    uint32_t* DetermineIndirect(on_disk::Inode& inode, unsigned int& inodeBlockNr, int& level)
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
        fs::Inode& inode, uint32_t* block, std::optional<bio::BufferRef> bio, const bool createIfNecessary)
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
        if (bio)
            bio::WriteBlock(std::move(*bio));

        // Zero new block content
        for (int n = 0; n < biosPerBlock; ++n) {
            auto newBIO = bio::ReadBlock(inode.dev, (newBlock * biosPerBlock) + n);
            assert(newBIO);
            memset(newBIO->data, 0, bio::BlockSize);
            bio::WriteBlock(std::move(newBIO));
        }
        return true;
    }

    uint32_t bmap(fs::Inode& inode, unsigned int inodeBlockNr, const bool createIfNecessary)
    {
        uint32_t ext2BlockNr = inodeBlockNr / biosPerBlock;
        uint32_t bioBlockOffset = inodeBlockNr % biosPerBlock;
        if (ext2BlockNr < 12) {
            auto block = &inode.ext2inode->i_block[ext2BlockNr];
            if (!AllocateNewBlockAsNecessary(inode, block, {}, createIfNecessary))
                return 0;
            return (*block * biosPerBlock) + bioBlockOffset;
        }

        int level;
        auto indirectPtr = DetermineIndirect(*inode.ext2inode, ext2BlockNr, level);
        if (!AllocateNewBlockAsNecessary(inode, indirectPtr, {}, createIfNecessary))
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
            auto buf = bio::ReadBlock(inode.dev, indirect);
            assert(buf);
            indirect = [&]() -> uint32_t {
                const auto blocks = reinterpret_cast<uint32_t*>(buf->data);
                auto blockPtr = &blocks[blockIndex];
                if (!AllocateNewBlockAsNecessary(inode, blockPtr, std::move(buf), createIfNecessary))
                    return 0;
                return *blockPtr;
            }();
        } while (--level >= 0);

        return indirect > 0 ? indirect * biosPerBlock + bioBlockOffset : 0;
    }

    bool ReadDirectory(fs::Inode& dirInode, off_t& offset, fs::DEntry& dentry)
    {
        union {
            on_disk::DirectoryEntry de;
            char block[bio::BlockSize];
        } u;

        while (offset < dirInode.ext2inode->i_size) {
            auto result = fs::Read(
                dirInode, reinterpret_cast<void*>(&u.block), offset,
                sizeof(on_disk::DirectoryEntry) + fs::MaxDirectoryEntryNameLength);
            if (!result || !*result)
                return false;
            int n = *result;

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

    result::MaybeInt WriteDirectoryEntry(
        fs::Inode& dirInode, off_t offset, fs::InodeNumber inum, uint16_t newEntryRecordLength,
        int type, const char* name)
    {
        union {
            on_disk::DirectoryEntry de;
            char block[bio::BlockSize];
        } newEntry;
        newEntry.de.inode = inum;
        newEntry.de.rec_len = newEntryRecordLength;
        newEntry.de.name_len = strlen(name);
        newEntry.de.file_type = type;
        memcpy(newEntry.de.name, name, newEntry.de.name_len);
        const auto entryLength = sizeof(on_disk::DirectoryEntry) + newEntry.de.name_len;
        return WriteExact(dirInode, reinterpret_cast<void*>(&newEntry), offset, entryLength);
    }

    result::MaybeInt AddEntryToDirectory(fs::Inode& dirInode, fs::InodeNumber inum, int type, const char* name)
    {
        const auto newEntryLength = RoundUpToMultipleOf4(sizeof(on_disk::DirectoryEntry) + strlen(name));
        off_t offset = 0;
        while (offset < dirInode.ext2inode->i_size) {
            on_disk::DirectoryEntry dentry;
            auto result = fs::Read(
                dirInode, reinterpret_cast<void*>(&dentry), offset, sizeof(on_disk::DirectoryEntry));
            if (!result) return result;
            if (!*result) break; // out of directory entries

            const auto currentEntryLength =
                dentry.inode != 0
                    ? RoundUpToMultipleOf4(sizeof(on_disk::DirectoryEntry) + dentry.name_len)
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
                auto result = WriteExact(dirInode, reinterpret_cast<void*>(&dentry), offset, sizeof(on_disk::DirectoryEntry));
                if (!result) return result;
                offset += dentry.rec_len;
            }

            // TODO undo previous changes on failure
            return WriteDirectoryEntry(dirInode, offset, inum, newEntryRecordLength, type, name);
        }

        const auto newEntryRecordLength = blockSize;
        return WriteDirectoryEntry(dirInode, offset, inum, newEntryRecordLength, type, name);
    }

    result::MaybeInt RemoveEntryFromDirectory(fs::Inode& dirInode, const char* name)
    {
        const auto nameLength = strlen(name);
        off_t offset = 0;
        off_t previousOffset = 0;
        on_disk::DirectoryEntry previousEntry{};
        while (offset < dirInode.ext2inode->i_size) {
            on_disk::DirectoryEntry dentry;
            char component[fs::MaxPathLength];
            if (auto result = ReadExact(dirInode, reinterpret_cast<void*>(&dentry), offset, sizeof(on_disk::DirectoryEntry)); !result) return result;

            if (auto result = ReadExact(dirInode, reinterpret_cast<void*>(component), offset + sizeof(on_disk::DirectoryEntry), dentry.name_len); !result) return result;

            if (dentry.name_len != nameLength || memcmp(component, name, nameLength) != 0) {
                previousOffset = offset;
                previousEntry = dentry;
                offset += dentry.rec_len;
                continue;
            }

            if (previousEntry.rec_len > 0) {
                previousEntry.rec_len += dentry.rec_len;
                auto result = fs::Write(
                           dirInode, reinterpret_cast<void*>(&previousEntry), previousOffset,
                           sizeof(on_disk::DirectoryEntry));
                if (!result) return result;
                if (*result == sizeof(on_disk::DirectoryEntry)) return 0;
                return result::Error(error::Code::IOError);
            }

            dentry.inode = 0;
            auto result = fs::Write(
                       dirInode, reinterpret_cast<void*>(&dentry), offset,
                       sizeof(on_disk::DirectoryEntry)) ;
            if (!result) return result;
            if (*result == sizeof(on_disk::DirectoryEntry)) return 0;
            return result::Error(error::Code::IOError);
        }
        return false;
    }

    result::MaybeInt CreateDirectory(fs::Inode& parent, const char* name, uint16_t mode)
    {
        auto result = CreateDirectoryEntry(parent, EXT2_S_IFDIR | mode, EXT2_FT_DIR, name);
        if (!result)
            return result::Error(result.error());
        auto newInode = std::move(*result);
        newInode->ext2inode->i_links_count = 2; // '.' and entry in parent

        // Write empty directory
        {
            ext2::on_disk::DirectoryEntry dirEntry{};
            dirEntry.rec_len = blockSize;
            if (fs::Write(*newInode, &dirEntry, 0, sizeof(dirEntry)) != sizeof(dirEntry))
                return result::Error(error::Code::OutOfSpace); // TODO undo damage
            newInode->ext2inode->i_size = blockSize;
        }
        fs::idirty(*newInode);

        if (!AddEntryToDirectory(*newInode, newInode->inum, EXT2_FT_DIR, ".")) {
            return result::Error(error::Code::OutOfSpace); // TODO deallocate inode
        }
        if (!AddEntryToDirectory(*newInode, parent.inum, EXT2_FT_DIR, "..")) {
            return result::Error(error::Code::OutOfSpace); // TODO deallocate inode
        }
        ++parent.ext2inode->i_links_count;
        fs::idirty(parent);

        UpdateInodeBlockGroup(
            newInode->dev, newInode->inum, [](auto& bg) { ++bg.bg_used_dirs_count; });
        return 0;
    }

    result::MaybeInt Unlink(fs::Inode& parent, const char* name)
    {
        return RemoveEntryFromDirectory(parent, name);
    }

    result::MaybeInt UnlinkInode(fs::InodeRef inode)
    {
        if (--inode->ext2inode->i_links_count > 0) {
            fs::idirty(*inode);
            return 0;
        }

        FreeDataBlocks(*inode);
        FreeInode(std::move(inode));
        return 0;
    }

    void Truncate(fs::Inode& inode)
    {
        // We always truncate to zero bytes...
        inode.ext2inode->i_size = 0;
        FreeDataBlocks(inode);
    }

    result::MaybeInt RemoveDirectory(fs::Inode& parent, fs::InodeRef inode)
    {
        if (!RemoveEntryFromDirectory(*inode, ".."))
            return result::Error(error::Code::IOError);
        if (!RemoveEntryFromDirectory(*inode, "."))
            return result::Error(error::Code::IOError);
        UpdateInodeBlockGroup(inode->dev, inode->inum, [](auto& bg) { --bg.bg_used_dirs_count; });

        FreeDataBlocks(*inode);
        FreeInode(std::move(inode));

        --parent.ext2inode->i_links_count;
        fs::idirty(parent);
        return 0;
    }

    result::Maybe<fs::InodeRef> CreateSpecial(fs::Inode& parent, const char* name, uint16_t mode, dev_t dev)
    {
        int ft = 0;
        switch(mode & EXT2_S_IFMASK) {
            case EXT2_S_IFBLK: ft = EXT2_FT_BLKDEV; break;
            case EXT2_S_IFCHR: ft = EXT2_FT_CHRDEV; break;
            default: return result::Error(error::Code::InvalidArgument);
        }

        auto result = CreateDirectoryEntry(parent, mode, ft, name);
        if (!result)
            return result::Error(result.error());
        auto newInode = std::move(*result);
        newInode->ext2inode->i_block[0] = static_cast<uint32_t>(dev);
        fs::idirty(*newInode);
        return newInode;
    }

    result::Maybe<fs::InodeRef> Mount(fs::Device dev)
    {
        // Piece the superblock together
        ReadBlocks(dev, 2, sizeof(on_disk::Superblock) / bio::BlockSize, &superblock);
        if (superblock.s_magic != constants::magic::Magic)
            return result::Error(error::Code::InvalidArgument);
        blockSize = 1024L << superblock.s_log_block_size;
        biosPerBlock = blockSize / bio::BlockSize;
        numberOfBlockGroups = (superblock.s_blocks_count - superblock.s_first_data_block) /
                              superblock.s_blocks_per_group;
        return fs::iget(dev, EXT2_ROOT_INO);
    }

    result::Maybe<stat> Stat(fs::Inode& inode)
    {
        assert(inode.ext2inode != nullptr);
        const auto& e2i = *inode.ext2inode;

        stat st{};
        st.st_dev = inode.dev;
        st.st_ino = inode.inum;
        st.st_mode = e2i.i_mode;
        st.st_uid = e2i.i_uid;
        st.st_size = e2i.i_size;
        st.st_atime = e2i.i_atime;
        st.st_ctime = e2i.i_ctime;
        st.st_mtime = e2i.i_mtime;
        st.st_gid = e2i.i_gid;
        st.st_nlink = e2i.i_links_count;
        st.st_blocks = e2i.i_blocks;
        return st;
    }

    result::Maybe<fs::InodeRef> CreateRegular(fs::Inode& parent, const char* name, uint16_t mode)
    {
        return CreateDirectoryEntry(parent, EXT2_S_IFREG | mode, EXT2_FT_REG_FILE, name);
    }

    result::Maybe<fs::InodeRef> CreateSymlink(fs::Inode& parent, const char* name, const char* target)
    {
        auto result = CreateDirectoryEntry(parent, EXT2_S_IFLNK | 0777, EXT2_FT_SYMLINK, name);
        if (!result)
            return result::Error(result.error());
        auto newInode = std::move(*result);
        if (fs::Write(*newInode, target, 0, strlen(target)) != strlen(target)) {
            // XXX undo damage
            return result::Error(error::Code::IOError);
        }
        return newInode;
    }

    result::MaybeInt CreateLink(fs::Inode& parent, fs::Inode& source, const char* name)
    {
        int ft = EXT2_FT_REG_FILE; // TODO
        if (!ext2::AddEntryToDirectory(parent, source.inum, ft, name)) {
            return result::Error(error::Code::OutOfSpace);
        }
        ++source.ext2inode->i_links_count;
        fs::idirty(source);
        return 0;
    }
} // namespace ext2
