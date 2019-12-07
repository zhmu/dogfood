#include "types.h"

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
        uint8_t data[BlockSize];
        Buffer* prev = nullptr;
        Buffer* next = nullptr;
        Buffer* qnext = nullptr;
    };

    void Initialize();
    Buffer& bread(int dev, BlockNumber blockNumber);
    void bwrite(Buffer& buf);
    void brelse(Buffer& buf);

} // namespace bio
