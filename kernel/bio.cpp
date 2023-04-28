#include "bio.h"
#include "lib.h"
#include "hw/ide.h"
#include <array>
#include <algorithm>

namespace bio
{
    namespace cache
    {
        inline constexpr unsigned int NumberOfBuffers = 50;
        Buffer buffer[NumberOfBuffers];
        // Circular list of all items in the cache
        Buffer head;

        namespace
        {
            void ClaimBuffer(Buffer& buffer)
            {
                // Place item between 'head' and head->next in the circular list;
                // note that 'head' always stays at the front
                buffer.next = cache::head.next;
                buffer.prev = &cache::head;

                cache::head.next->prev = &buffer;
                cache::head.next = &buffer;
            }
        } // namespace
    }     // namespace cache

    namespace
    {
        struct BlockDevice
        {
            int device = -1;
            uint64_t first_lba = -1;
        };
        std::array<BlockDevice, 4> blockDevice;

        bool CommitBuffer(Buffer& buffer)
        {
            if ((buffer.flags & flag::Dirty) == 0) return false;
            ide::PerformIO(buffer);
            return true;
        }
    }

    BlockDevice* FindBlockDevice(int device)
    {
        auto it = std::find_if(blockDevice.begin(), blockDevice.end(), [&](const auto& v) {
            return v.device == device;
        });
        return it != blockDevice.end() ? &*it : nullptr;
    }

    void RegisterDevice(int device, uint64_t first_lba)
    {
        auto d = FindBlockDevice(-1);
        assert(d != nullptr);
        *d = { device, first_lba };
    }

    void Initialize()
    {
        cache::head.next = &cache::head;
        cache::head.prev = &cache::head;
        for (auto& buffer : cache::buffer) {
            cache::ClaimBuffer(buffer);
        }
    }

    BufferRef bget(int dev, BlockNumber blockNumber)
    {
        // Look through the cache; we skip head as it doesn't contain anything useful
        for (auto buf = cache::head.next; buf != &cache::head; buf = buf->next) {
            if (buf->dev == dev && buf->blockNumber == blockNumber) {
                ++buf->refCount;
                return BufferRef{buf};
            }
        }

        // Sacrifice the least-recently used block (walks circular list backwards)
        for (auto buf = cache::head.prev; buf != &cache::head; buf = buf->prev) {
            if (buf->refCount == 0) {
                CommitBuffer(*buf);
                auto bdev = FindBlockDevice(dev);
                assert(bdev != nullptr);
                buf->dev = dev;
                buf->blockNumber = blockNumber;
                buf->ioBlockNumber = bdev->first_lba + blockNumber;
                buf->flags = 0; // not valid, not dirty either
                buf->refCount = 1;
                return BufferRef{buf};
            }
        }

        panic("bget: out of buffers");
    }

    BufferRef ReadBlock(int dev, BlockNumber blockNumber)
    {
        auto buf = bget(dev, blockNumber);
        if (buf && (buf->flags & flag::Valid) == 0)
            ide::PerformIO(*buf);
        return buf;
    }

    void WriteBlock(BufferRef buf)
    {
        assert(buf);
        buf->flags |= flag::Dirty;
    }

    namespace detail
    {
        void ReleaseBuffer(Buffer& buf)
        {
            if (--buf.refCount == 0) {
                // No longer in use; move to the cache
                buf.prev->next = buf.next;
                buf.next->prev = buf.prev;
                cache::ClaimBuffer(buf);
            }
        }
    }

    int Sync()
    {
        int n = 0;
        for (auto buf = cache::head.next; buf != &cache::head; buf = buf->next) {
            if (CommitBuffer(*buf)) ++n;
        }
        return n;
    }

} // namespace bio
