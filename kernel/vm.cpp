#include "vm.h"
#include "x86_64/amd64.h"
#include "x86_64/paging.h"
#include "lib.h"
#include "page_allocator.h"
#include "process.h"
#include "syscall.h"
#include <dogfood/vmop.h>
#include <dogfood/errno.h>

static constexpr inline auto DEBUG_VM = false;

namespace vm
{
    namespace
    {
        uint64_t AllocateNewPage()
        {
            auto new_page = page_allocator::Allocate();
            memset(new_page, 0, vm::PageSize);
            return vm::VirtualToPhysical(new_page) | Page_P | Page_US | Page_RW;
        }

        bool HandleMappingPageFault(process::Process& proc, const uint64_t virt)
        {
            auto va = RoundDownToPage(virt);
            for(auto& mapping: proc.mappings) {
                if (va < mapping.va_start || va >= mapping.va_end) continue;

                void* page = page_allocator::Allocate();
                if (page == nullptr)
                    return false;
                memset(page, 0, vm::PageSize);

                const auto readOffset = va - mapping.va_start;
                int bytesToRead = vm::PageSize;
                if (readOffset + bytesToRead > mapping.inode_length)
                    bytesToRead = mapping.inode_length - readOffset;

                if constexpr (DEBUG_VM) {
                    Print(
                        "HandleMappingPageFault: proc ", proc.pid, " va ", print::Hex{virt},
                        " inum ", mapping.inode->inum,
                        " offset ", print::Hex{mapping.inode_offset + readOffset}, ", ",
                        bytesToRead, " bytes\n");
                }
                if (bytesToRead > 0 &&
                    fs::Read(*mapping.inode, page, mapping.inode_offset + readOffset, bytesToRead) != bytesToRead) {
                    page_allocator::Free(page);
                    return false;
                }

                auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(proc.pageDirectory));
                vm::Map(pml4, va, vm::PageSize, vm::VirtualToPhysical(page), mapping.pte_flags);
                return true;
            }
            return false;
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
            auto pte = amd64::paging::FindPTE(pml4, va, AllocateNewPage);
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
            page_allocator::Free(amd64::paging::MakePointerToEntry(entry));
        };

        amd64::paging::WalkPTE(pml4, freePageIfInUserLand, freePageIfInUserLand);
        page_allocator::Free(pml4);
    }

    uint64_t* CloneMappings(uint64_t* src_pml4)
    {
        uint64_t* dst_pml4 = CreateUserlandPageDirectory();
        assert(dst_pml4 != nullptr);

        amd64::paging::WalkPTE(
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
        auto vmopArg = syscall::GetArgument<1, VMOP_OPTIONS*>(tf);
        auto vmop = *vmopArg;
        if (!vmop) { return -EFAULT; }

        auto& current = process::GetCurrent();
        switch (vmop->vo_op) {
            case OP_MAP: {
                // XXX no inode-based mappings just yet
                if ((vmop->vo_flags & (VMOP_FLAG_PRIVATE | VMOP_FLAG_FD | VMOP_FLAG_FIXED)) !=
                    VMOP_FLAG_PRIVATE)
                    return -EINVAL;

                vmop->vo_addr = reinterpret_cast<void*>(current.nextMmapAddress);
                auto pd = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
                for (int n = 0; n < vmop->vo_len / vm::PageSize; ++n) {
                    vm::Map(
                        pd, current.nextMmapAddress, vm::PageSize, 0, vm::Page_RW | vm::Page_US);
                    current.nextMmapAddress += vm::PageSize;
                }
                if (!vmopArg.Set(*vmop)) return -EFAULT;
                return 0;
            }
            case OP_UNMAP: {
                auto va = reinterpret_cast<uint64_t>(vmop->vo_addr);
                if (va < vm::userland::mmapBase)
                    return -EINVAL;
                if (va >= current.nextMmapAddress)
                    return -EINVAL;
                if ((va & ~(vm::PageSize - 1)) != 0)
                    return -EINVAL;

                auto pd = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
                for (int n = 0; n < vm::RoundUpToPage(vmop->vo_size) / vm::PageSize; ++n) {
                    auto pte = amd64::paging::FindPTE(pd, va, [] { return 0; });
                    if (pte == nullptr)
                        return -EINVAL;
                    page_allocator::Free(amd64::paging::MakePointerToEntry(*pte));
                    *pte = 0;
                }
                return 0;
            }
            default:
                Print(
                    "vmop: unimplemented op ", vmop->vo_op, " addr ", vmop->vo_addr,
                    " len ", print::Hex{vmop->vo_len}, "\n");
                return -ENODEV;
        }
        return -1;
    }

    bool HandlePageFault(uint64_t va, int errnum)
    {
        auto& current = process::GetCurrent();
        if (HandleMappingPageFault(current, va))
            return true;

        auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
        auto pte = amd64::paging::FindPTE(pml4, vm::RoundDownToPage(va), [] { return 0; });
        if (pte == nullptr)
            return false;
        if ((*pte & Page_P) != 0)
            return false;
        if ((*pte & Page_US) == 0)
            return false;

        auto page = page_allocator::Allocate();
        if (page == nullptr)
            return false;
        memset(page, 0, vm::PageSize);

        *pte |= vm::VirtualToPhysical(page) | Page_P;
        return true;
    }
} // namespace vm
