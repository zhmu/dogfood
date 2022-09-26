#pragma once

#include "../vm.h"

namespace amd64::paging
{
    inline uint64_t* MakePointerToEntry(const uint64_t entry)
    {
        return reinterpret_cast<uint64_t*>(::vm::PhysicalToVirtual(entry & 0xffffffffff000));
    }

    template<typename Fn>
    uint64_t* CreateOrGetPage(uint64_t& entry, Fn createFn)
    {
        if ((entry & vm::Page_P) == 0) {
            entry = createFn();
            if (!entry)
                return nullptr;
        }
        return MakePointerToEntry(entry);
    }

    template<typename Fn>
    uint64_t* FindPTE(uint64_t* pml4, uint64_t addr, Fn createFn)
    {
        const auto pml4Offset = (addr >> 39) & 0x1ff;
        const auto pdpeOffset = (addr >> 30) & 0x1ff;
        const auto pdpOffset = (addr >> 21) & 0x1ff;
        const auto pteOffset = (addr >> 12) & 0x1ff;

        auto pdpe = CreateOrGetPage(pml4[pml4Offset], createFn);
        if (pdpe == nullptr)
            return nullptr;
        auto pdp = CreateOrGetPage(pdpe[pdpeOffset], createFn);
        if (pdp == nullptr)
            return nullptr;
        auto pte = CreateOrGetPage(pdp[pdpOffset], createFn);
        if (pte == nullptr)
            return nullptr;
        return &pte[pteOffset];
    }

    inline constexpr uint64_t CombineAddressPieces(
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
            if ((pml4[pml4eOffset] & vm::Page_P) == 0)
                continue;
            auto pdpe = MakePointerToEntry(pml4[pml4eOffset]);
            for (uint64_t pdpeOffset = 0; pdpeOffset < 512; ++pdpeOffset) {
                if ((pdpe[pdpeOffset] & vm::Page_P) == 0)
                    continue;
                auto pdp = MakePointerToEntry(pdpe[pdpeOffset]);
                for (uint64_t pdpOffset = 0; pdpOffset < 512; ++pdpOffset) {
                    if ((pdp[pdpOffset] & vm::Page_P) == 0)
                        continue;
                    auto pte = MakePointerToEntry(pdp[pdpOffset]);
                    for (uint64_t pteOffset = 0; pteOffset < 512; ++pteOffset) {
                        if ((pte[pteOffset] & vm::Page_P) == 0)
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
}
