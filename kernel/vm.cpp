#include "vm.h"
#include "amd64.h"
#include "lib.h"
#include "page_allocator.h"

namespace vm
{
    namespace
    {
        uint64_t* MakePointerToEntry(const uint64_t entry)
        {
            return reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(entry & 0xffffffffff000));
        }

        uint64_t* CreateOrGetPage(uint64_t& entry, const bool create)
        {
            if ((entry & Page_P) == 0) {
                if (!create)
                    return nullptr;
                auto new_page = page_allocator::Allocate();
                memset(new_page, 0, vm::PageSize);
                entry = vm::VirtualToPhysical(new_page) | Page_P | Page_US | Page_RW;
            }
            return MakePointerToEntry(entry);
        }

        uint64_t* FindPTE(uint64_t* pml4, uint64_t addr, bool create)
        {
            const auto pml4Offset = (addr >> 39) & 0x1ff;
            const auto pdpeOffset = (addr >> 30) & 0x1ff;
            const auto pdpOffset = (addr >> 21) & 0x1ff;
            const auto pteOffset = (addr >> 12) & 0x1ff;

            auto pdpe = CreateOrGetPage(pml4[pml4Offset], create);
            if (pdpe == nullptr)
                return nullptr;
            auto pdp = CreateOrGetPage(pdpe[pdpeOffset], create);
            if (pdp == nullptr)
                return nullptr;
            auto pte = CreateOrGetPage(pdp[pdpOffset], create);
            if (pte == nullptr)
                return nullptr;
            return &pte[pteOffset];
        }

        template<typename Func>
        void WalkPTE(uint64_t* pml4, Func func)
        {
            for (uint64_t pml4eOffset = 0; pml4eOffset < 512; ++pml4eOffset) {
                if ((pml4[pml4eOffset] & Page_P) == 0)
                    continue;
                auto pdpe = MakePointerToEntry(pml4[pml4eOffset]);
                for (uint64_t pdpeOffset = 0; pdpeOffset < 512; ++pdpeOffset) {
                    if ((pdpe[pdpeOffset] & Page_P) == 0)
                        continue;
                    auto pdp = MakePointerToEntry(pdpe[pdpeOffset]);
                    for (uint64_t pdpOffset = 0; pdpOffset < 512; ++pdpOffset) {
                        if ((pdp[pdpOffset] & Page_P) == 0)
                            continue;
                        auto pte = MakePointerToEntry(pdp[pdpOffset]);
                        for (uint64_t pteOffset = 0; pteOffset < 512; ++pteOffset) {
                            if ((pte[pteOffset] & Page_P) == 0)
                                continue;
                            auto va = (pteOffset << 12) | (pdpOffset << 21) | (pdpeOffset << 30) |
                                      (pml4eOffset << 39);
                            if (va & (1UL << 47))
                                va |= 0xffff000000000000; // sign-extend to canonical-address form
                            func(va, pte[pteOffset]);
                        }
                    }
                }
            }
        }
    } // namespace

    void
    Map(uint64_t* pml4, const uint64_t va_start, const size_t length, const uint64_t phys,
        const uint64_t pteFlags)
    {
        auto va = RoundDownToPage(va_start);
        auto pa = phys;
        const auto va_end = RoundDownToPage(va_start + length - 1);
        do {
            auto pte = FindPTE(pml4, va, true);
            assert(pte != nullptr);
            *pte = phys | pteFlags;
            pa += PageSize;
            va += PageSize;
        } while (va < va_end);
    }

    uint64_t* CreateUserlandPageDirectory()
    {
        auto pml4 = reinterpret_cast<uint64_t*>(page_allocator::Allocate());
        assert(pml4 != nullptr);
        memcpy(pml4, kernel_pagedir, vm::PageSize);
        return pml4;
    }

    void FreeUserlandPageDirectory(uint64_t* pml4)
    {
        WalkPTE(pml4, [](const uint64_t va, const uint64_t entry) {
            if (va >= vm::PhysicalToVirtual(static_cast<uint64_t>(0)))
                return;
            page_allocator::Free(MakePointerToEntry(entry));
        });
        page_allocator::Free(pml4);
    }
} // namespace vm
