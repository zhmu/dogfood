#pragma once

#include "types.h"
#include "intrusive_list.h"

namespace page_allocator
{
    struct PageZone;

    struct Page : util::intrusive_list_node {
        PageZone& zone;
        unsigned int order;

        Page(PageZone& zone, unsigned int order) : zone(zone), order(order) {}

        char* GetData();
        uint64_t GetPhysicalAddress();
    };

    void RegisterMemory(char* base, const unsigned int length_in_pages);
    uint64_t GetNumberOfAvailablePages();

    Page* AllocateOrder(int order);
    Page* AllocateOne();
    void Free(Page& p);
} // namespace page_allocator
