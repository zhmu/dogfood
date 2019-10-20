#include "vm.h"
#include "amd64.h"
#include "errno.h"
#include "lib.h"
#include "page_allocator.h"
#include "process.h"
#include "syscall.h"
#include "syscall-types.h"

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

        constexpr uint64_t CombineAddressPieces(
            uint64_t pteOffset, uint64_t pdpOffset, uint64_t pdpeOffset, uint64_t pml4eOffset)
        {
            auto addr =
                (pteOffset << 12) | (pdpOffset << 21) | (pdpeOffset << 30) | (pml4eOffset << 39);
            if (addr & (1UL << 47))
                addr |= 0xffff000000000000; // sign-extend to canonical-address form
            return addr;
        }

        template<typename OnIndirectionPage, typename OnMapping>
        void WalkPTE(uint64_t* pml4, OnIndirectionPage onIndirectionPage, OnMapping onMapping)
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
                            const auto va =
                                CombineAddressPieces(pteOffset, pdpOffset, pdpeOffset, pml4eOffset);
                            onMapping(va, pte[pteOffset]);
                        }
                        const auto va = CombineAddressPieces(0, pdpOffset, pdpeOffset, pml4eOffset);
                        onIndirectionPage(va, pdp[pdpOffset]);
                    }
                    const auto va = CombineAddressPieces(0, 0, pdpeOffset, pml4eOffset);
                    onIndirectionPage(va, pdpe[pdpeOffset]);
                }
                const auto va = CombineAddressPieces(0, 0, 0, pml4eOffset);
                onIndirectionPage(va, pml4[pml4eOffset]);
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
        auto freePageIfInUserLand = [](const uint64_t va, const uint64_t entry) {
            if (va >= vm::PhysicalToVirtual(static_cast<uint64_t>(0)))
                return;
            page_allocator::Free(MakePointerToEntry(entry));
        };

        WalkPTE(pml4, freePageIfInUserLand, freePageIfInUserLand);
        page_allocator::Free(pml4);
    }

    uint64_t* CloneMappings(uint64_t* src_pml4)
    {
        uint64_t* dst_pml4 = CreateUserlandPageDirectory();
        assert(dst_pml4 != nullptr);

        WalkPTE(
            src_pml4, [](const auto, const auto) {},
            [&](const uint64_t va, const uint64_t entry) {
                if (va >= vm::PhysicalToVirtual(static_cast<uint64_t>(0)))
                    return;

                auto page = page_allocator::Allocate();
                assert(page != nullptr);
                memcpy(
                    page, reinterpret_cast<const void*>(vm::VirtualToPhysical(va)), vm::PageSize);
                auto pteFlags = entry & 0xfff;
                if (entry & Page_NX)
                    pteFlags |= Page_NX;
                Map(dst_pml4, va, vm::PageSize, vm::VirtualToPhysical(page), pteFlags);
            });
        return dst_pml4;
    }

    long VmOp(amd64::TrapFrame& tf)
    {
        auto vmop = reinterpret_cast<VMOP_OPTIONS*>(syscall::GetArgument<1>(tf));

        auto& current = process::GetCurrent();
        switch (vmop->vo_op) {
            case OP_SBRK: {
                if (vmop->vo_len < 0)
                    return -EINVAL;
                const auto previousHeapSize = current.heapSize;
                current.heapSize += vmop->vo_len;

                auto pml4 =
                    reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
                while (current.heapSizeAllocated < vm::RoundUpToPage(current.heapSize)) {
                    auto pte = FindPTE(pml4, vm::userland::heapBase + current.heapSizeAllocated, true);
                    if (pte == nullptr) return -ENOMEM;
                    auto page = page_allocator::Allocate();
                    if (page == nullptr) return -ENOMEM;
                    memset(page, 0, vm::PageSize);

                    *pte = vm::Page_P | vm::Page_RW | vm::Page_US | vm::VirtualToPhysical(page);
                    current.heapSizeAllocated += vm::PageSize;
                }
                vmop->vo_addr = reinterpret_cast<void*>(vm::userland::heapBase + previousHeapSize);
                return 0;
            }
            default:
                 printf("vmop: unimplemented op %d addr %p len %p\n", vmop->vo_op, vmop->vo_addr, vmop->vo_len);
                return -EINVAL;
        }
        return -1;
    }

    bool HandlePageFault(uint64_t va, int errnum)
    {
        auto& current = process::GetCurrent();
        auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
        auto pte = FindPTE(pml4, vm::RoundDownToPage(va), false);
        if (pte == nullptr) return false;
        if ((*pte & Page_P) != 0) return false;
        if ((*pte & Page_US) == 0) return false;

        auto page = page_allocator::Allocate();
        if (page == nullptr) return false;
        memset(page, 0, vm::PageSize);

        *pte |= vm::VirtualToPhysical(page) | Page_P;
        return true;
    }
} // namespace vm
