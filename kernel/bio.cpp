#include "bio.h"
#include "lib.h"
#include "hw/ide.h"

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

    void Initialize()
    {
        cache::head.next = &cache::head;
        cache::head.prev = &cache::head;
        for (auto& buffer : cache::buffer) {
            cache::ClaimBuffer(buffer);
        }
    }

    Buffer& bget(int dev, BlockNumber blockNumber)
    {
        // Look through the cache; we skip head as it doesn't contain anything useful
        for (auto buf = cache::head.next; buf != &cache::head; buf = buf->next) {
            if (buf->dev == dev && buf->blockNumber == blockNumber) {
                ++buf->refCount;
                return *buf;
            }
        }

        // Sacrifice the least-recently used block (walks circular list backwards)
        for (auto buf = cache::head.prev; buf != &cache::head; buf = buf->prev) {
            if (buf->refCount == 0 && (buf->flags & flag::Dirty) == 0) {
                buf->dev = dev;
                buf->blockNumber = blockNumber;
                buf->flags = 0; // not valid, not dirty either
                buf->refCount = 1;
                return *buf;
            }
        }

        panic("bget: out of buffers");
    }

    Buffer& bread(int dev, BlockNumber blockNumber)
    {
        auto& buf = bget(dev, blockNumber);
        if ((buf.flags & flag::Valid) == 0)
            ide::PerformIO(buf);
        return buf;
    }

    void bwrite(Buffer& buf)
    {
        buf.flags |= flag::Dirty;
        ide::PerformIO(buf);
    }

    void brelse(Buffer& buf)
    {
        if (--buf.refCount == 0) {
            // No longer in use; move to the cache
            buf.prev->next = buf.next;
            buf.next->prev = buf.prev;
            cache::ClaimBuffer(buf);
        }
    }

} // namespace bio
