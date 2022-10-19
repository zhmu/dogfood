/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */

/*
 * Implementation of a page allocator using a buddy system; a thorough
 * explanation can be found in The Art Of Computer Programming Volume 1,
 * 2.5 "Dynamic Storage Allocation".
 */

#include <array>
#include <numeric>
#include "vm.h"
#include "lib.h"
#include "page_allocator.h"
#include "intrusive_list.h"

namespace page_allocator
{
    constexpr auto inline DEBUG_PAGE_ALLOC = false;
    constexpr auto inline MaxOrders = 10;

    using PageList = util::intrusive_list<Page>;

    struct PageZone : util::intrusive_list_node {
        std::array<PageList, MaxOrders> free;

        const unsigned int num_pages;
        char* const first_page_data;
        unsigned int avail_pages = 0;
        Page* base;
        uint8_t* bitmap;

        PageZone(char* first_page_data, unsigned int num_pages)
            : first_page_data(first_page_data), num_pages(num_pages)
        {
        }
    };

    char* Page::GetData()
    {
        assert(order >= 0 && order < MaxOrders);
        const auto index = this - zone.base;
        return &zone.first_page_data[index * vm::PageSize];
    }

    uint64_t Page::GetPhysicalAddress()
    {
        return vm::VirtualToPhysical(GetData());
    }

    namespace
    {
        util::intrusive_list<PageZone> zones;

        template<typename... Args>
        void Debug(Args&&... args)
        {
            if constexpr (DEBUG_PAGE_ALLOC) {
                Print(std::forward<Args>(args)...);
            }
        }

        int IsPageInUse(uint8_t* map, int bit) { return (map[bit / 8] & (1 << (bit & 7))) != 0; }

        void MarkPageInUse(uint8_t* map, int bit) { map[bit / 8] |= 1 << (bit & 7); }

        void MarkPageAsFree(uint8_t* map, int bit) { map[bit / 8] &= ~(1 << (bit & 7)); }

        PageRef AllocateFromZone(PageZone& z, unsigned int order)
        {
            Debug("AllocateFromZone(): z=", &z, "order=", order, "\n");

            // First step is to figure out the initial order we need to use
            unsigned int alloc_order = order;
            while (alloc_order < MaxOrders && z.free[alloc_order].empty())
                ++alloc_order; // Nothing free here
            Debug(
                "AllocateFromZone(): z=", &z, ", order=", order, " -> alloc_order=", alloc_order, "\n");
            if (alloc_order == MaxOrders)
                return nullptr;

            // Split pages until we reach our target page order
            while (alloc_order > order) {
                assert(!z.free[alloc_order].empty());
                auto& p = z.free[alloc_order].front();
                z.free[alloc_order].pop_front();

                const auto page_index = &p - z.base;
                const auto buddy_index = page_index ^ (1 << (alloc_order - 1));
                Debug(
                    "AllocateFromZone(): alloc_order=", alloc_order, ", splitting index ", page_index, " -> ", page_index, ", ",
                    buddy_index, "\n");
                Debug("split page0=", &z.base[page_index], ", page1=", &z.base[buddy_index], "\n");
                --alloc_order;
                z.free[alloc_order].push_back(z.base[page_index]);
                z.free[alloc_order].push_back(z.base[buddy_index]);
                z.base[page_index].order = alloc_order;
                z.base[buddy_index].order = alloc_order;
            }

            // Grab a page from the order's free list
            assert(!z.free[order].empty());
            auto& p = z.free[order].front();
            z.free[order].pop_front();
            z.avail_pages -= 1 << order;

            // Properly fill out the page's fields
            assert(p.order == order);
            const auto prev_refcount = p.refcount.exchange(1);
            assert(prev_refcount == 0);
            const auto page_index = &p - z.base;
            MarkPageInUse(z.bitmap, page_index);
            Debug("page_alloc_zone(): got page=", &p, " index ", page_index, " refcount ", p.refcount.load(), "\n");
            return PageRef(&p);
        }

        void FreeIndex(PageZone& z, unsigned int order, unsigned int index)
        {
            Page& p = z.base[index];
            Debug("FreeIndex(): order=", order, " index=", index, " -> p=", &p, "\n");

            // Make page available
            MarkPageAsFree(z.bitmap, index);
            z.avail_pages += 1 << order;
            z.free[order].push_back(p);

            // Attempt to merge the available pages
            while (order < MaxOrders - 1) {
                const auto buddy_index = index ^ (1 << order);
                if (buddy_index >= z.num_pages || IsPageInUse(z.bitmap, buddy_index))
                    break; // buddy not free, bail out

                if (z.base[buddy_index].order != order)
                    break; // buddy is of different order than this block, can't merge

                Debug(
                    "FreeIndex(): order=", order, ", index=", index, ", buddy free ", buddy_index,
                    "\n");

                // Merge by removing both pages from this order...
                z.free[order].remove(z.base[index]);
                z.free[order].remove(z.base[buddy_index]);

                // ... and combining them as a single entry one order above
                ++order;
                index &= ~((1 << order) - 1);
                z.free[order].push_back(z.base[index]);
                z.base[index].order = order;
            }
        }
    } // namespace

    namespace detail {
        void Deref(Page& p)
        {
            assert(p.order >= 0 && p.order < MaxOrders);

            const auto prev_refcount = p.refcount.fetch_sub(1);
            Debug("Deref(): page ", &p, " refcount ", prev_refcount, "\n");
            assert(prev_refcount > 0);
            if (prev_refcount == 1)
                FreeIndex(p.zone, p.order, &p - p.zone.base);
        }
    }

    void RegisterMemory(char* base, unsigned int num_pages)
    {
        const auto bitmap_size = (num_pages + 7) / 8;
        const auto num_admin_pages =
            (sizeof(PageZone) + bitmap_size + (num_pages * sizeof(Page)) + vm::PageSize - 1) /
            vm::PageSize;
        Debug(
            "RegisterZone: base=", base, " num_pages=", num_pages,
            ", num_admin_pages=", num_admin_pages, "\n");

        // Initialize the page zone; initially, we'll just mark everything as allocated
        PageZone& z = *reinterpret_cast<PageZone*>(base);
        const auto first_page_data = base + num_admin_pages * vm::PageSize;
        new (&z) PageZone(first_page_data, num_pages - num_admin_pages);
        z.bitmap = reinterpret_cast<uint8_t*>(&base[sizeof(z)]);
        memset(z.bitmap, 0xff, bitmap_size); // all pages in use
        z.base = reinterpret_cast<Page*>(base + bitmap_size + sizeof(z));

        // Create the page structures; we mark everything as a order 0 page
        auto p = z.base;
        for (unsigned int n = 0; n < z.num_pages; ++n, ++p) {
            new (p) Page(z, 0);
        }

        /*
         * Free all chunks of memory. This is slow, we could do better but for
         * now it'll help guarantee that the implementation is correct.
         */
        for (int n = 0; n < z.num_pages; n++)
            FreeIndex(z, 0, n);

        zones.push_back(z);
    }

    PageRef AllocateOrder(int order)
    {
        assert(order >= 0 && order < MaxOrders);

        for (auto& z : zones) {
            auto page = AllocateFromZone(z, order);
            if (page)
                return page;
        }

        panic("out of pages!");
    }

    PageRef AllocateOne()
    {
        return AllocateOrder(0);
    }

    PageRef AddReference(PageRef& page)
    {
        const auto prev_refcount = page->refcount.fetch_add(1);
        assert(prev_refcount > 0);
        return PageRef(page.get());
    }

    uint64_t GetNumberOfAvailablePages()
    {
        return std::accumulate(zones.begin(), zones.end(), 0, [](auto n, const auto& z) {
            return n + z.avail_pages;
        });
    }
} // namespace page_allocator
