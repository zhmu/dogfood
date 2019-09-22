#include "vm.h"
#include "amd64.h"
#include "lib.h"
#include "page_allocator.h"

namespace vm
{
    namespace {
        uint64_t* CreateOrGetPage(uint64_t& entry, bool create)
        {
            if ((entry & Page_P) == 0) {
                if (!create) return nullptr;
                auto new_page = page_allocator::Allocate();
                memset(new_page, 0, vm::PageSize);
                entry = vm::VirtualToPhysical(new_page) | Page_P | Page_US | Page_RW;
            }
            return reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(entry & 0xffffffffff000));
        }
    }

    uint64_t* FindPTE(uint64_t* pml4, uint64_t addr, bool create)
    {
        const auto pml4Offset = (addr >> 39) & 0x1ff;
        const auto pdpeOffset = (addr >> 30) & 0x1ff;
        const auto pdpOffset = (addr >> 21) & 0x1ff;
        const auto pteOffset = (addr >> 12) & 0x1ff;

        auto pdpe = CreateOrGetPage(pml4[pml4Offset], create);
        if (pdpe == nullptr) return nullptr;
        auto pdp = CreateOrGetPage(pdpe[pdpeOffset], create);
        if (pdp == nullptr) return nullptr;
        auto pte = CreateOrGetPage(pdp[pdpOffset], create);
        if (pte == nullptr) return nullptr;
        return &pte[pteOffset];
    }

    void Map(
        uint64_t* pml4, const uint64_t va_start, const size_t length, const uint64_t phys, const uint64_t pteFlags)
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
}
