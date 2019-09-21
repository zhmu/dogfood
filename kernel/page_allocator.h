#include "types.h"

namespace page_allocator
{
    void RegisterMemory(const uint64_t base, const unsigned int length_in_pages);

    void* Allocate();
    void Free(void*);
    uint64_t GetNumberOfAvailablePages();
} // namespace page_allocator
