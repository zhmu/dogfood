#pragma once

#include "types.h"
#include "intrusive_list.h"
#include <atomic>
#include <memory>

namespace page_allocator
{
    struct PageZone;

    /*
     * A range of pages of memory as managed by the kernel. All physical memory
     * is always mapped, and pages can be shared between processes as needed.
     *
     * The number of consequent pages held by this structure is '1 << order', which
     * hold 'vm::PageSize << order' bytes of data.
     */
    struct Page : util::intrusive_list_node {
        PageZone& zone;
        // Order determines the page size, must be >=0
        int order{-1};
        // Manual refcount as shared_ptr<> will dynamically allocate the
        // control block - must be >0 for any page in use
        std::atomic<int> refcount{0};

        Page(PageZone& zone, unsigned int order) : zone(zone), order(order) {}

        char* GetData();
        uint64_t GetPhysicalAddress();
    };

    namespace detail {
        void Deref(Page& p); // Do not call - PageRef uses this as needed
    }

    struct PageDeref {
        void operator()(Page* p) { detail::Deref(*p); }
    };

    // Contains a reference to a page; will automatically dereference the page
    // as needed
    using PageRef = std::unique_ptr<Page, PageDeref>;

    void RegisterMemory(char* base, const unsigned int length_in_pages);
    uint64_t GetNumberOfAvailablePages();

    PageRef AllocateOrder(int order);
    PageRef AllocateOne();
    PageRef AddReference(PageRef& page);

} // namespace page_allocator
