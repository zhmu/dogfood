#include "gtest/gtest.h"
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include "stub.h"

#include "ext2-image.cpp"

#define stat dfstat
#undef st_atime
#undef st_mtime
#undef st_ctime
#include "dogfood/stat.h"
#include "../kernel/bio.h"
#include "../kernel/ext2.h"
#include "../kernel/fs.h"

namespace {
    constexpr inline fs::InodeNumber rootInodeNumber = 2;
    constexpr inline fs::InodeNumber file1InodeNumber = 14;
    constexpr inline fs::InodeNumber file2InodeNumber = 15;
    constexpr inline fs::InodeNumber symlinkInodeNumber = 16;
    constexpr inline uid_t defaultUid = 1000;
    constexpr inline uid_t defaultGid = 1000;
    constexpr inline int deviceNumber = 0;

    constexpr inline unsigned int blockSize = 2048;
    constexpr inline unsigned int bioBlocksPerExt2Block = blockSize / bio::BlockSize;

    constexpr unsigned int Ext2ToBioBlockNumber(unsigned int blockNr)
    {
        return blockNr * bioBlocksPerExt2Block;
    }

    template<typename T>
    std::vector<unsigned int> ConvertExt2BlocksToBioBlocks(const T& ext2Blocks)
    {
        std::vector<unsigned int> blocks;
        for(const auto ext2Block: ext2Blocks) {
            const auto bioBlockBase = Ext2ToBioBlockNumber(ext2Block);
            for(unsigned int n = 0; n < bioBlocksPerExt2Block; ++n) {
                blocks.push_back(bioBlockBase > 0 ? bioBlockBase + n : 0);
            }
        }
        return blocks;
    }

    void InitializeDependencies() {
        test_stubs::SetPutCharFunction([](int n) { putchar(n); });
        bio::Initialize();
        fs::Initialize(); // XXX this also mounts, TODO
    }

    fs::Inode MakeFSInode(fs::InodeNumber inum, ext2::Inode& ext2Inode)
    {
        return fs::Inode{ deviceNumber, inum, 1, false, &ext2Inode };
    }

    void VerifyRootInode(const ext2::Inode& ext2inode)
    {
        EXPECT_EQ(0, ext2inode.i_uid);
        EXPECT_EQ(0, ext2inode.i_gid);
        EXPECT_EQ(2048, ext2inode.i_size);
        EXPECT_EQ(EXT2_S_IFDIR | 0755, ext2inode.i_mode);
    }

    std::vector<fs::DEntry> ReadDirectoryEntries(fs::Inode& inode)
    {
        std::vector<fs::DEntry> dirEntries;

        off_t dirOffset = 0;
        fs::DEntry dentry;
        while (ext2::ReadDirectory(inode, dirOffset, dentry)) {
            dirEntries.push_back(dentry);
        }
        return dirEntries;
    }

    template<typename T>
    unsigned int CountNumberOfSetBits(const T value)
    {
        unsigned int result = 0;
        for(unsigned int n = 0; n < std::numeric_limits<T>::digits; ++n) {
            if (value & (1ULL << n)) ++result;
        }
        return result;
    }

    struct Update { size_t offset; uint8_t oldValue; uint8_t newValue; };

    template<typename C, typename T>
    bool IsFieldUpdated(const Update& u, T C::* field)
    {
        const auto start = reinterpret_cast<size_t>(&(reinterpret_cast<C*>(0)->*field));
        const auto end = start + sizeof(T);
        const auto offset = u.offset % sizeof(C);
        return offset >= start && offset < end;
    }

    struct IOProvider
    {
        IOProvider()
            : image(GenerateImage())
        {
            test_stubs::SetPerformIOFunction([this](auto& buffer) { PerformIO(buffer); });
        }

        ~IOProvider()
        {
            test_stubs::SetPerformIOFunction(nullptr);
        }

        void PerformIO(bio::Buffer& buffer)
        {
            const size_t offset = buffer.blockNumber * bio::BlockSize;
            ASSERT_LE(offset + bio::BlockSize, image.size());

            if ((buffer.flags & bio::flag::Valid) == 0) {
                std::copy(image.begin() + offset, image.begin() + offset + bio::BlockSize, buffer.data);
                buffer.flags |= bio::flag::Valid;
            }

            if ((buffer.flags & bio::flag::Dirty) != 0) {
                for(int n = 0; n < bio::BlockSize; ++n) {
                    if(buffer.data[n] == image[offset + n]) continue;
                    updates.push_back({ offset + n, image[offset + n], buffer.data[n] });
                    image[offset + n] = buffer.data[n];
                }
                buffer.flags &= ~bio::flag::Dirty;
            }
        }

        std::vector<uint8_t> image;
        std::vector<Update> updates;
    };

    struct Ext2 : ::testing::Test
    {
        Ext2() {
            InitializeDependencies();
        }

        ~Ext2() {
            InitializeDependencies();
        }

        IOProvider io;
    };
}

TEST(Ext2Test, CountNumberOfSetBits)
{
    EXPECT_EQ(0, CountNumberOfSetBits(static_cast<uint8_t>(0)));
    EXPECT_EQ(4, CountNumberOfSetBits(static_cast<uint8_t>(0x55)));
    EXPECT_EQ(4, CountNumberOfSetBits(static_cast<uint8_t>(0xaa)));
    EXPECT_EQ(8, CountNumberOfSetBits(static_cast<uint8_t>(-1)));

    EXPECT_EQ(0, CountNumberOfSetBits(static_cast<uint32_t>(0)));
    EXPECT_EQ(16, CountNumberOfSetBits(static_cast<uint32_t>(0x5555aaaa)));
    EXPECT_EQ(32, CountNumberOfSetBits(static_cast<uint32_t>(-1)));
}

TEST(Ext2Test, IsFieldUpdated)
{
    struct Test { uint8_t a; uint16_t b; uint32_t c; } __attribute__((packed));

    EXPECT_TRUE(IsFieldUpdated({ 0, 0, 0 }, &Test::a));
    EXPECT_FALSE(IsFieldUpdated({ 1, 0, 0 }, &Test::a));
    EXPECT_TRUE(IsFieldUpdated({ 1, 0, 0 }, &Test::b));
    EXPECT_TRUE(IsFieldUpdated({ 2, 0, 0 }, &Test::b));
    EXPECT_FALSE(IsFieldUpdated({ 3, 0, 0 }, &Test::b));
    EXPECT_TRUE(IsFieldUpdated({ 3, 0, 0 }, &Test::c));
    EXPECT_TRUE(IsFieldUpdated({ 4, 0, 0 }, &Test::c));
    EXPECT_TRUE(IsFieldUpdated({ 5, 0, 0 }, &Test::c));
    EXPECT_TRUE(IsFieldUpdated({ 6, 0, 0 }, &Test::c));
    EXPECT_FALSE(IsFieldUpdated({ 7, 0, 0 }, &Test::c));
}

TEST_F(Ext2, Initialize)
{
}

TEST_F(Ext2, Mount)
{
    auto inode = ext2::Mount(deviceNumber);
    ASSERT_NE(nullptr, inode);
    EXPECT_EQ(rootInodeNumber, inode->inum);
    VerifyRootInode(*inode->ext2inode);
}

TEST_F(Ext2, ReadInode_Root)
{
    auto rootInode = ext2::Mount(deviceNumber);

    ext2::Inode inode;
    ext2::ReadInode(deviceNumber, rootInodeNumber, inode);
    VerifyRootInode(inode);
}

TEST_F(Ext2, ReadInode_File1)
{
    auto rootInode = ext2::Mount(deviceNumber);

    ext2::Inode inode;
    ext2::ReadInode(deviceNumber, file1InodeNumber, inode);
    EXPECT_EQ(defaultUid, inode.i_uid);
    EXPECT_EQ(defaultGid, inode.i_gid);
    EXPECT_EQ(1337, inode.i_size);
    EXPECT_EQ(EXT2_S_IFREG | 0644, inode.i_mode);
}

TEST_F(Ext2, ReadInode_File2)
{
    auto rootInode = ext2::Mount(deviceNumber);

    ext2::Inode inode;
    ext2::ReadInode(deviceNumber, file2InodeNumber, inode);
    EXPECT_EQ(defaultUid, inode.i_uid);
    EXPECT_EQ(defaultGid, inode.i_gid);
    EXPECT_EQ(65536, inode.i_size);
    EXPECT_EQ(EXT2_S_IFREG | 0644, inode.i_mode);
}

TEST_F(Ext2, Bmap_Read_Root)
{
    auto rootInode = ext2::Mount(deviceNumber);

    const auto rootBlockNumber = ConvertExt2BlocksToBioBlocks(std::array{
        20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    });
    for(unsigned int n = 0; n < static_cast<int>(rootBlockNumber.size()); ++n) {
        EXPECT_EQ(rootBlockNumber[n], ext2::bmap(*rootInode, n, false));
    }
}

TEST_F(Ext2, Bmap_Read_File1)
{
    (void)ext2::Mount(deviceNumber);
    ext2::Inode ext2Inode;
    ext2::ReadInode(deviceNumber, file1InodeNumber, ext2Inode);
    auto fsInode = MakeFSInode(file1InodeNumber, ext2Inode);

    const auto blocks = ConvertExt2BlocksToBioBlocks(std::array{
        32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    });
    for(unsigned int n = 0; n < static_cast<int>(blocks.size()); ++n) {
        EXPECT_EQ(blocks[n], ext2::bmap(fsInode, n, false));
    }
}

TEST_F(Ext2, Bmap_Read_File2)
{
    (void)ext2::Mount(deviceNumber);
    ext2::Inode ext2Inode;
    ext2::ReadInode(deviceNumber, file2InodeNumber, ext2Inode);
    auto fsInode = MakeFSInode(file2InodeNumber, ext2Inode);

    const auto blocks = ConvertExt2BlocksToBioBlocks(std::array{
        0, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 49, 50,
        51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 0
    });
    for(unsigned int n = 0; n < static_cast<int>(blocks.size()); ++n) {
        EXPECT_EQ(blocks[n], ext2::bmap(fsInode, n, false));
    }
}

TEST_F(Ext2, ReadDirectory_Root)
{
    auto rootInode = ext2::Mount(deviceNumber);
    auto dirEntries = ReadDirectoryEntries(*rootInode);

    struct ExpectedEntry { fs::InodeNumber inum; const char* name; };
    const std::array expectedEntries{
        ExpectedEntry{ rootInodeNumber, "." },
        ExpectedEntry{ rootInodeNumber, ".." },
        ExpectedEntry{ 11, "lost+found" },
        ExpectedEntry{ 12, "dir1" },
        ExpectedEntry{ 13, "dir2" },
        ExpectedEntry{ file1InodeNumber, "file1.txt" },
        ExpectedEntry{ file2InodeNumber, "file2.bin" },
        ExpectedEntry{ symlinkInodeNumber, "symlink" },
    };
    ASSERT_EQ(expectedEntries.size(), dirEntries.size());
    for(size_t n = 0; n < expectedEntries.size(); ++n) {
        EXPECT_EQ(expectedEntries[n].inum, dirEntries[n].d_ino);
        EXPECT_EQ(0, strcmp(expectedEntries[n].name, dirEntries[n].d_name));
    }
}

TEST_F(Ext2, WriteInode_File1_NoChange)
{
    (void)ext2::Mount(deviceNumber);

    ext2::Inode ext2Inode;
    ext2::ReadInode(deviceNumber, file1InodeNumber, ext2Inode);
    auto fsInode = MakeFSInode(file1InodeNumber, ext2Inode);
    ext2::WriteInode(fsInode);

    EXPECT_TRUE(io.updates.empty());
}

TEST_F(Ext2, WriteInode_File1_Change)
{
    (void)ext2::Mount(deviceNumber);

    ext2::Inode ext2Inode;
    ext2::ReadInode(deviceNumber, file1InodeNumber, ext2Inode);
    auto fsInode = MakeFSInode(file1InodeNumber, ext2Inode);
    ext2Inode.i_size = 1339;
    ext2::WriteInode(fsInode);

    // Only one byte changed (1337 -> 1339)
    ASSERT_EQ(static_cast<size_t>(1), io.updates.size());
    EXPECT_TRUE(IsFieldUpdated(io.updates[0], &ext2::Inode::i_size));
    EXPECT_EQ(1337 & 255, io.updates[0].oldValue);
    EXPECT_EQ(1339 & 255, io.updates[0].newValue);
}

TEST_F(Ext2, AllocateInode_Single)
{
    {
        auto rootInode = ext2::Mount(deviceNumber);
        const auto newInodeNumber = ext2::AllocateInode(*rootInode);
        EXPECT_GT(newInodeNumber, rootInodeNumber);
    }

    ASSERT_EQ(static_cast<size_t>(3), io.updates.size());
    {
        // Inode bitmap: one additional bit is to be set
        const auto oldNumberOfBits = CountNumberOfSetBits(io.updates[0].oldValue);
        const auto newNumberOfBits = CountNumberOfSetBits(io.updates[0].newValue);
        EXPECT_EQ(1, newNumberOfBits - oldNumberOfBits);
    }

    // Block group: one less inode is available
    EXPECT_TRUE(IsFieldUpdated(io.updates[1], &ext2::BlockGroup::bg_free_inodes_count));
    EXPECT_EQ(1, io.updates[1].oldValue - io.updates[1].newValue);
    // Superblock: one less inode is available
    EXPECT_TRUE(IsFieldUpdated(io.updates[2], &ext2::Superblock::s_free_inodes_count));
    EXPECT_EQ(1, io.updates[2].oldValue - io.updates[2].newValue);
}
