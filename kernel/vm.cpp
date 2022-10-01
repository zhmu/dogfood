#include "vm.h"
#include "x86_64/amd64.h"
#include "x86_64/paging.h"
#include "lib.h"
#include "page_allocator.h"
#include "process.h"
#include "syscall.h"
#include <dogfood/vmop.h>
#include <dogfood/errno.h>

#include <algorithm>

static constexpr inline auto DEBUG_VM = false;

namespace vm
{
    namespace
    {
        uint64_t AllocateMDPage(process::Process& proc)
        {
            auto new_page = page_allocator::Allocate();
            assert(new_page != nullptr);
            proc.mdPages.push_back(new_page);
            memset(new_page, 0, vm::PageSize);
            return vm::VirtualToPhysical(new_page) | Page_P | Page_US | Page_RW;
        }

        bool HandleMappingPageFault(process::Process& proc, const uint64_t virt)
        {
            const auto va = RoundDownToPage(virt);
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

                mapping.pages.push_back({ va, page });

                vm::MapMemory(proc, va, vm::PageSize, vm::VirtualToPhysical(page), mapping.pte_flags);
                return true;
            }
            return false;
        }

        void ClonePage(process::Process& destProc, process::Mapping& destMapping, const process::Page& p)
        {
            auto newP{p};
            newP.page = page_allocator::Allocate();
            assert(newP.page != nullptr);
            memcpy(newP.page, p.page, vm::PageSize);
            MapMemory(destProc, newP.va, vm::PageSize, vm::VirtualToPhysical(newP.page), destMapping.pte_flags);
            destMapping.pages.push_back(newP);

            using namespace print;
        }

        int ConvertVmopFlags(int opflags)
        {
            int flags = vm::Page_US | vm::Page_P;
            if (opflags & VMOP_FLAG_WRITE)
                flags |= vm::Page_RW;
            if ((opflags & VMOP_FLAG_EXECUTE) == 0)
                flags |= vm::Page_NX;
            return flags;
        }
    } // namespace

    void Dump(process::Process& proc)
    {
        using namespace print;
        Print("vm::Dump() pid ", proc.pid, " - start\n");
        for(const auto& m: proc.mappings) {
            if (m.pte_flags == 0) continue;
            Print("  area ", Hex{m.va_start}, " .. ", Hex{m.va_end}, "\n");
            for(auto& p: m.pages) {
                Print("    va ", Hex{p.va}, " page ", p.page, "\n");
            }
        }
        Print("vm::Dump() pid ", proc.pid, " - end\n");
    }

    char* CreateAndMapUserStack(process::Process& proc)
    {
        auto ustack = reinterpret_cast<char*>(page_allocator::Allocate());
        assert(ustack != nullptr);
        memset(ustack, 0, vm::PageSize);

        Map(proc, vm::userland::stackBase, vm::Page_P | vm::Page_RW | vm::Page_US, vm::PageSize);
        MapMemory(proc,
            vm::userland::stackBase, vm::PageSize, vm::VirtualToPhysical(ustack),
            vm::Page_P | vm::Page_RW | vm::Page_US);
        return ustack;
    }

    void
    MapMemory(process::Process& proc, const uint64_t va_start, const size_t length, const uint64_t phys,
        const uint64_t pteFlags)
    {
        auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(proc.pageDirectory));
        auto va = RoundDownToPage(va_start);
        auto pa = phys;
        const auto va_end = RoundDownToPage(va_start + length - 1);
        do {
            auto pte = amd64::paging::FindPTE(pml4, va, [&]() {
                return AllocateMDPage(proc);
            });
            assert(pte != nullptr);
            *pte = phys | pteFlags;
            pa += PageSize;
            va += PageSize;
        } while (va < va_end);
    }

    uint64_t* CreateUserlandPageDirectory(process::Process& proc)
    {
        assert(proc.pageDirectory == 0);
        assert(proc.mdPages.size() == 1);
        assert(proc.mdPages.front() == proc.kernelStack);

        auto pml4 = reinterpret_cast<uint64_t*>(page_allocator::Allocate());
        assert(pml4 != nullptr);
        memcpy(pml4, kernel_pagedir, vm::PageSize);
        proc.pageDirectory = vm::VirtualToPhysical(pml4);
        proc.mdPages.push_back(pml4);
        return pml4;
    }

    void DestroyUserlandPageDirectory(process::Process& proc)
    {
        assert(&process::GetCurrent() != &proc);
        assert(proc.mappings.empty());
        for(auto& p: proc.mdPages) {
            page_allocator::Free(p);
        }
        proc.mdPages.clear();
        proc.pageDirectory = 0;
    }

    void FreeMappings(process::Process& proc)
    {
        for(auto& m: proc.mappings) {
            if (m.inode != nullptr)
                fs::iput(*m.inode);
            for(auto& p: m.pages) {
                page_allocator::Free(p.page);
                MapMemory(proc, p.va, vm::PageSize, 0, 0);
            }
        }
        proc.mappings.clear();
    }

    void CloneMappings(process::Process& destProc)
    {
        auto& sourceProc = process::GetCurrent();

        for(const auto& sourceMapping: sourceProc.mappings) {
            destProc.mappings.push_back(sourceMapping);
            auto& destMapping = destProc.mappings.back();
            destMapping = sourceMapping;
            if (destMapping.inode) fs::iref(*destMapping.inode);
            destMapping.pages.clear();

            for(const auto& p: sourceMapping.pages) {
                ClonePage(destProc, destMapping, p);
            }
        }
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

                // TODO search/overwrite mappings etc
                auto& mapping = Map(current, current.nextMmapAddress, ConvertVmopFlags(vmop->vo_flags), vmop->vo_len);
                current.nextMmapAddress = mapping.va_end;

                vmop->vo_addr = reinterpret_cast<void*>(mapping.va_start);
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


                // TODO search/overwrite mappings etc
                Print("todo OP_UNMAP\n");
                return -EINVAL;
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
        return HandleMappingPageFault(current, va);
    }

    process::Mapping& Map(process::Process& proc, uint64_t va, uint64_t pteFlags, uint64_t mappingSize)
    {
        process::Mapping mapping;
        mapping.va_start = va;
        mapping.va_end = va + mappingSize;
        mapping.pte_flags = pteFlags;
        proc.mappings.push_back(mapping);
        return proc.mappings.back();
    }

    process::Mapping& MapInode(process::Process& proc, uint64_t va, uint64_t pteFlags, uint64_t mappingSize, fs::Inode& inode, uint64_t inodeOffset, uint64_t inodeSize)
    {
        auto& mapping = Map(proc, va, pteFlags, mappingSize);
        mapping.inode = &inode;
        mapping.inode_offset = inodeOffset;
        mapping.inode_length = inodeSize;
        fs::iref(*mapping.inode);
        return mapping;
    }

} // namespace vm
