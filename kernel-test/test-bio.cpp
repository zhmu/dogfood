#include "gtest/gtest.h"
#include <sys/types.h>
#include <vector>
#include "stub.h"
#include "../kernel/bio.h"

namespace {
    template<typename Operation>
    void ConstructSectorData(uint8_t* data, Operation op)
    {
        for(int n = 0; n < bio::BlockSize; ++n)
            data[n] = op(n) & 0xff;
    }

    void CreateSectorTestContent(const int deviceNumber, const bio::BlockNumber blockNr, uint8_t* data)
    {
        ConstructSectorData(data, [&](auto n) {
            return (deviceNumber ^ blockNr) + n;
        });
    }

    struct IOWrapper
    {
        IOWrapper()
        {
            test_stubs::SetPerformIOFunction([this](auto& buffer) { PerformIO(buffer); });
        }

        ~IOWrapper()
        {
            test_stubs::SetPerformIOFunction(nullptr);
        }

        void PerformIO(bio::Buffer& buffer)
        {
            operations.push_back(Operation{buffer.dev, buffer.blockNumber, buffer.flags});
            if ((buffer.flags & bio::flag::Valid) == 0) {
                CreateSectorTestContent(buffer.dev, buffer.blockNumber, buffer.data);
                buffer.flags |= bio::flag::Valid;
            }
        }

        struct Operation {
            int deviceNumber{};
            bio::BlockNumber blockNumber{};
            int flags{};
        };

        std::vector<Operation> operations;
    };

    struct Bio : ::testing::Test
    {
        Bio() {
            test_stubs::ResetFunctions();
            bio::Initialize();
        }
    };

    void VerifyOperation(const IOWrapper::Operation& op, const int deviceNumber, const bio::BlockNumber blockNumber, const int flags)
    {
        EXPECT_EQ(deviceNumber, op.deviceNumber);
        EXPECT_EQ(flags, op.flags);
        EXPECT_EQ(blockNumber, op.blockNumber);
    }

    void VerifySectorContent(const bio::Buffer& buffer, int deviceNumber, const bio::BlockNumber blockNumber)
    {
        uint8_t expectedData[bio::BlockSize];
        CreateSectorTestContent(deviceNumber, blockNumber, expectedData);
        EXPECT_EQ(0, memcmp(buffer.data, expectedData, bio::BlockSize));
    }
}

TEST_F(Bio, Initialize)
{
}

TEST_F(Bio, Bread_One_Sector)
{
    IOWrapper ioWrapper;

    constexpr int deviceNumber = 0;
    constexpr bio::BlockNumber blockNumber = 1234;

    auto& buffer = bio::bread(deviceNumber, blockNumber);
    ASSERT_EQ(static_cast<size_t>(1), ioWrapper.operations.size());
    VerifyOperation(ioWrapper.operations.front(), deviceNumber, blockNumber, 0);
    VerifySectorContent(buffer, deviceNumber, blockNumber);

    bio::brelse(buffer);
}

TEST_F(Bio, Bread_Same_Sector_Twice_From_Same_Device)
{
    IOWrapper ioWrapper;

    constexpr int deviceNumber = 0;
    constexpr bio::BlockNumber blockNumber = 5678;

    auto& buffer1 = bio::bread(deviceNumber, blockNumber);
    auto& buffer2 = bio::bread(deviceNumber, blockNumber);
    EXPECT_EQ(&buffer1, &buffer2);
    EXPECT_EQ(buffer1.data, buffer2.data);
    ASSERT_EQ(static_cast<size_t>(1), ioWrapper.operations.size());

    VerifyOperation(ioWrapper.operations.front(), deviceNumber, blockNumber, 0);
    VerifySectorContent(buffer1, deviceNumber, blockNumber);

    bio::brelse(buffer1);
    bio::brelse(buffer2);
}

TEST_F(Bio, Bread_Same_Sector_From_Different_Devices)
{
    IOWrapper ioWrapper;

    constexpr int deviceNumber1 = 0;
    constexpr int deviceNumber2 = 1;
    constexpr bio::BlockNumber blockNumber = 18728;

    auto& buffer1 = bio::bread(deviceNumber1, blockNumber);
    auto& buffer2 = bio::bread(deviceNumber2, blockNumber);
    EXPECT_NE(&buffer1, &buffer2);
    EXPECT_NE(buffer1.data, buffer2.data);
    ASSERT_EQ(static_cast<size_t>(2), ioWrapper.operations.size());

    VerifyOperation(ioWrapper.operations[0], deviceNumber1, blockNumber, 0);
    VerifyOperation(ioWrapper.operations[1], deviceNumber2, blockNumber, 0);
    VerifySectorContent(buffer1, deviceNumber1, blockNumber);
    VerifySectorContent(buffer2, deviceNumber2, blockNumber);

    bio::brelse(buffer2);
    bio::brelse(buffer1);
}

TEST_F(Bio, Bread_Different_Sectors_From_Same_Device)
{
    IOWrapper ioWrapper;

    constexpr int deviceNumber = 0;
    constexpr bio::BlockNumber blockNumber1 = 1782;
    constexpr bio::BlockNumber blockNumber2 = 8912;

    auto& buffer1 = bio::bread(deviceNumber, blockNumber1);
    auto& buffer2 = bio::bread(deviceNumber, blockNumber2);
    EXPECT_NE(&buffer1, &buffer2);
    EXPECT_NE(buffer1.data, buffer2.data);
    ASSERT_EQ(static_cast<size_t>(2), ioWrapper.operations.size());

    VerifyOperation(ioWrapper.operations[0], deviceNumber, blockNumber1, 0);
    VerifyOperation(ioWrapper.operations[1], deviceNumber, blockNumber2, 0);
    VerifySectorContent(buffer1, deviceNumber, blockNumber1);
    VerifySectorContent(buffer2, deviceNumber, blockNumber2);

    bio::brelse(buffer2);
    bio::brelse(buffer1);
}

TEST_F(Bio, Bread_Different_Sectors_From_Different_Devices)
{
    IOWrapper ioWrapper;

    constexpr int deviceNumber1 = 0;
    constexpr int deviceNumber2 = 1;
    constexpr bio::BlockNumber blockNumber1 = 2872;
    constexpr bio::BlockNumber blockNumber2 = 2981;

    auto& buffer1 = bio::bread(deviceNumber1, blockNumber1);
    auto& buffer2 = bio::bread(deviceNumber2, blockNumber2);
    EXPECT_NE(&buffer1, &buffer2);
    ASSERT_EQ(static_cast<size_t>(2), ioWrapper.operations.size());

    VerifyOperation(ioWrapper.operations[0], deviceNumber1, blockNumber1, 0);
    VerifyOperation(ioWrapper.operations[1], deviceNumber2, blockNumber2, 0);
    VerifySectorContent(buffer1, deviceNumber1, blockNumber1);
    VerifySectorContent(buffer2, deviceNumber2, blockNumber2);

    bio::brelse(buffer2);
    bio::brelse(buffer1);
}

TEST_F(Bio, Bwrite_One_Sector)
{
    IOWrapper ioWrapper;

    constexpr int deviceNumber = 0;
    constexpr bio::BlockNumber blockNumber = 8837;

    uint8_t testContent[bio::BlockSize];
    ConstructSectorData(testContent, [](int n) { return ~n; });

    auto& buffer = bio::bread(deviceNumber, blockNumber);
    ioWrapper.operations.clear(); // don't care about these now
    memcpy(buffer.data, testContent, bio::BlockSize);
    bio::bwrite(buffer);

    ASSERT_EQ(static_cast<size_t>(1), ioWrapper.operations.size());
    VerifyOperation(ioWrapper.operations.front(), deviceNumber, blockNumber, bio::flag::Valid | bio::flag::Dirty);
    EXPECT_EQ(0, memcmp(testContent, buffer.data, bio::BlockSize));

    bio::brelse(buffer);
}
