/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
// This implements a simple memory allocater based on the "The C Programming
// Language" by Kernighan and Ritchie Section 8.7: Example - A Storage
// Allocator".
//
// heap::InitializeHeap() must be called once to provide the memory manager
// with the heap buffer it is allowed to allocate from. This buffer cannot be
// extended after it has been set up.
#include "heap.h"
#include <cstddef>
#include <algorithm>

namespace
{
    struct Header {
        Header* h_next;
        size_t h_size;
    };
    static_assert(alignof(Header) >= alignof(long));

    Header base{};       /* empty list to get started */
    Header* freelist = nullptr; /* start of free list */

    void Free(void*);

    void Initialize(void* data, const size_t number_of_bytes)
    {
        // Initialize the freelist
        base.h_next = &base;
        base.h_size = 0;
        freelist = &base;

        auto h = reinterpret_cast<Header*>(data);
        h->h_size = number_of_bytes / sizeof(Header);
        // Call Free() to add the new memory to the freelist
        Free(static_cast<void*>(h + 1));
    }

    void Free(void* ap)
    {
        auto bp = static_cast<Header*>(ap) - 1;

        // Point to block header
        auto p = freelist;
        for (/* nothing */; !(bp > p && bp < p->h_next); p = p->h_next) {
            if (p >= p->h_next && (bp > p || bp < p->h_next)) {
                // Freed block at start or end of arena
                break;
            }
        }

        if (bp + bp->h_size == p->h_next) {
            /* join to upper nbr */
            bp->h_size += p->h_next->h_size;
            bp->h_next = p->h_next->h_next;
        } else
            bp->h_next = p->h_next;

        if (p + p->h_size == bp) {
            /* join to lower nbr */
            p->h_size += bp->h_size;
            p->h_next = bp->h_next;
        } else
            p->h_next = bp;

        freelist = p;
    }

    void* Allocate(size_t nbytes)
    {
        const auto nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;
        auto prevp = freelist;

        for (auto p = prevp->h_next; /* nothing */; prevp = p, p = p->h_next) {
            if (p->h_size >= nunits) {
                if (p->h_size == nunits) {
                    // Fits exactly
                    prevp->h_next = p->h_next;
                } else {
                    // Allocate tail end
                    p->h_size -= nunits;
                    p += p->h_size;
                    p->h_size = nunits;
                }
                freelist = prevp;
                return static_cast<void*>(p + 1);
            }
            if (p == freelist) {
                // Wrapped around free list; we are out of space
                return nullptr;
            }
        }
    }
} // unnamed namespace

namespace heap {

    void InitializeHeap(void* ptr, size_t number_of_bytes)
    {
        Initialize(ptr, number_of_bytes);
    }

}

void* operator new(size_t len) { return Allocate(len); }
void* operator new[](size_t len) { return Allocate(len); }

void operator delete(void* p) noexcept { Free(p); }
void operator delete[](void* p) noexcept { Free(p); }
void operator delete(void* p, size_t) noexcept { Free(p); }
void operator delete[](void* p, size_t) noexcept { Free(p); }
