#include "types.h"
#include <memory>

namespace bio
{
    inline constexpr unsigned int BlockSize = 512;
    using BlockNumber = uint64_t;

    namespace flag
    {
        inline constexpr unsigned int Valid = 1;
        inline constexpr unsigned int Dirty = 2;
    } // namespace flag

    struct Buffer {
        int dev{};
        int flags{};
        int refCount{};
        BlockNumber blockNumber{};
        BlockNumber ioBlockNumber{};
        uint8_t data[BlockSize];
        Buffer* prev = nullptr;
        Buffer* next = nullptr;
        Buffer* qnext = nullptr;

        Buffer() = default;
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
    };

    namespace detail {
        void ReleaseBuffer(Buffer& buf);
    }

    struct BufferDeref {
        void operator()(Buffer* bio) { detail::ReleaseBuffer(*bio); }
    };
    using BufferRef = std::unique_ptr<Buffer, BufferDeref>;

    void Initialize();
    void RegisterDevice(int device, uint64_t first_lba);

    BufferRef ReadBlock(int dev, BlockNumber blockNumber);
    void WriteBlock(BufferRef buf);

} // namespace bio
