#include "gtest/gtest.h"
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include "stub.h"

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


    struct IOProvider
    {
        IOProvider()
        {
            test_stubs::SetPerformIOFunction([this](auto& buffer) { PerformIO(buffer); });

            fd = open("/work/test-ext2-img/ext2.img", O_RDONLY);
            if (fd < 0)
                fd = open("/home/rink/github/dogfood/test-ext2-img/ext2.img", O_RDONLY);
            if (fd < 0) throw std::runtime_error("cannot open disk image");
        }

        ~IOProvider()
        {
            close(fd);
            test_stubs::SetPerformIOFunction(nullptr);
        }

        void PerformIO(bio::Buffer& buffer)
        {
            if ((buffer.flags & bio::flag::Valid) == 0) {
                const auto bytesRead = pread(fd, buffer.data, bio::BlockSize, buffer.blockNumber * bio::BlockSize);
                EXPECT_EQ(bio::BlockSize, bytesRead);
            }
        }

        int fd;
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
        printf("block %d: exp %d got %d\n",
        n, blocks[n], ext2::bmap(fsInode, n, false));
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
