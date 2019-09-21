#include "page_allocator.h"
#include "amd64.h"

#include "lib.h"

namespace page_allocator
{
    namespace
    {
        struct FreeList {
            FreeList* next;
        };

        FreeList* freelist = nullptr;
    } // namespace

    void RegisterMemory(const uint64_t base, const unsigned int length_in_pages)
    {
        uint64_t p = base;
        for (unsigned int n = 0; n < length_in_pages; ++n) {
            Free(reinterpret_cast<void*>(p));
            p += amd64::PageSize;
        }
    }

    void* Allocate()
    {
        auto ptr = freelist;
        if (ptr != nullptr)
            freelist = freelist->next;
        return reinterpret_cast<void*>(ptr);
    }

    void Free(void* p)
    {
        auto ptr = static_cast<FreeList*>(p);
        ptr->next = freelist;
        freelist = ptr;
    }

    uint64_t GetNumberOfAvailablePages()
    {
        uint64_t count = 0;
        for (auto ptr = freelist; ptr != nullptr; ptr = ptr->next)
            ++count;
        return count;
    }

} // namespace page_allocator
