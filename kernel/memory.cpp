#include "lib.h"
#include "page_allocator.h"
#include "vm.h"

// This implements a simple memory allocater based on the "The C Programming
// Language" by Kernighan and Ritchie Section 8.7: Example - A Storage
// Allocator".
//
// Note that we cannot allocate more than a single page, as we can only request
// a single page from the page allocator.
namespace
{
    struct Header {
        Header* h_next;
        size_t h_size;
    };
    static_assert(alignof(Header) >= alignof(long));

    constexpr auto unitsPerPage = vm::PageSize / sizeof(Header);
    Header base{};       /* empty list to get started */
    Header* freelist = nullptr; /* start of free list */

    void Free(void*);

    Header* ClaimPageForAllocator()
    {
        auto p = page_allocator::Allocate();
        if (p == nullptr)
            return nullptr; // out of space

        auto h = static_cast<Header*>(p);
        h->h_size = unitsPerPage;
        // Call Free() to add the new memory to the freelist
        Free(static_cast<void*>(h + 1));
        return freelist;
    }

    void Free(void* ap)
    {
        assert(ap);
        auto bp = static_cast<Header*>(ap) - 1;

        // Point to block header
        auto p = freelist;
        for (/* nothing */; !(bp > p && bp < p->h_next); p = p->h_next)
            if (p >= p->h_next && (bp > p || bp < p->h_next)) {
                // Freed block at start or end of arena
                break;
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
        assert(nunits < unitsPerPage);
        auto prevp = freelist;
        if (prevp == nullptr) {
            // No free list yet
            base.h_next = &base;
            base.h_size = 0;
            freelist = &base;
            prevp = &base;
        }

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
                // Wrapped around free list; try to increase backing store
                p = ClaimPageForAllocator();
                if (!p)
                    return nullptr; /* none left */
            }
        }
    }
} // unnamed namespace

void* operator new(size_t len) { return Allocate(len); }
void* operator new[](size_t len) { return Allocate(len); }

void operator delete(void* p) noexcept { Free(p); }
void operator delete(void* p, size_t) noexcept { Free(p); }
