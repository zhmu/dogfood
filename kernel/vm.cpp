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
static constexpr inline uint64_t initCodeBase = 0x8000000;

extern "C" void* initcode;
extern "C" void* initcode_end;
extern amd64::PageDirectory kernel_pagedir;

namespace vm
{
    namespace
    {
        bool IsActive(VMSpace& vs)
        {
            return amd64::read_cr3() == vs.pageDirectory;
        }

        VMSpace& GetCurrent()
        {
            return process::GetCurrent().vmspace;
        }

        uint64_t AllocateMDPage(VMSpace& vs)
        {
            auto new_page = page_allocator::Allocate();
            assert(new_page != nullptr);
            vs.mdPages.push_back(new_page);
            memset(new_page, 0, vm::PageSize);
            return vm::VirtualToPhysical(new_page) | Page_P | Page_US | Page_RW;
        }

        bool HandleMappingPageFault(VMSpace& vs, const uint64_t virt)
        {
            const auto va = RoundDownToPage(virt);
            for(auto& mapping: vs.mappings) {
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
                        "HandleMappingPageFault: va ", print::Hex{virt},
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

                MapMemory(vs, va, vm::PageSize, vm::VirtualToPhysical(page), mapping.pte_flags);
                return true;
            }
            return false;
        }

        void ClonePage(VMSpace& vs, Mapping& destMapping, const Page& p)
        {
            auto newP{p};
            newP.page = page_allocator::Allocate();
            assert(newP.page != nullptr);
            memcpy(newP.page, p.page, vm::PageSize);
            MapMemory(vs, newP.va, vm::PageSize, vm::VirtualToPhysical(newP.page), destMapping.pte_flags);
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

    char* CreateKernelStack(VMSpace& vs)
    {
        auto kstack = reinterpret_cast<char*>(page_allocator::Allocate());
        assert(kstack != nullptr);
        vs.mdPages.push_back(kstack);
        vs.kernelStack = kstack;
        return kstack + vm::PageSize;
    }

    void Dump(VMSpace& vs)
    {
        using namespace print;
        for(const auto& m: vs.mappings) {
            if (m.pte_flags == 0) continue;
            Print("  area ", Hex{m.va_start}, " .. ", Hex{m.va_end}, "\n");
            for(auto& p: m.pages) {
                Print("    va ", Hex{p.va}, " page ", p.page, "\n");
            }
        }
    }

    void
    MapMemory(VMSpace& vs, const uint64_t va_start, const size_t length, const uint64_t phys,
        const uint64_t pteFlags)
    {
        auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(vs.pageDirectory));
        auto va = RoundDownToPage(va_start);
        auto pa = phys;
        const auto va_end = RoundDownToPage(va_start + length - 1);
        do {
            auto pte = amd64::paging::FindPTE(pml4, va, [&]() {
                return AllocateMDPage(vs);
            });
            assert(pte != nullptr);
            *pte = phys | pteFlags;
            pa += PageSize;
            va += PageSize;
        } while (va < va_end);
    }

    void InitializeVMSpace(VMSpace& vs)
    {
        assert(vs.pageDirectory == 0);
        assert(vs.mdPages.size() == 1);
        assert(vs.mdPages.front() == vs.kernelStack);

        auto pml4 = reinterpret_cast<uint64_t*>(page_allocator::Allocate());
        assert(pml4 != nullptr);
        memcpy(pml4, kernel_pagedir, vm::PageSize);
        vs.pageDirectory = vm::VirtualToPhysical(pml4);
        vs.mdPages.push_back(pml4);
        vs.nextMmapAddress = vm::userland::mmapBase;
    }

    void DestroyVMSpace(VMSpace& vs)
    {
        assert(!IsActive(vs));
        assert(vs.mappings.empty());
        for(auto& p: vs.mdPages) {
            page_allocator::Free(p);
        }
        vs.mdPages.clear();
        vs.pageDirectory = 0;
    }

    void SetupForInitProcess(VMSpace& vs, amd64::TrapFrame& tf)
    {
        // Set up userland stack; it will be paged in as needed
        Map(vs, vm::userland::stackBase, vm::Page_P | vm::Page_RW | vm::Page_US, vm::PageSize);
        tf.rsp = vm::userland::stackBase + vm::PageSize;

        // Fill a page with code to execve("/sbin/init", ...)
        auto code = reinterpret_cast<uint8_t*>(page_allocator::Allocate());
        vs.mdPages.push_back(code);
        memcpy(code, &initcode, (uint64_t)&initcode_end - (uint64_t)&initcode);
        MapMemory(vs,
            initCodeBase, vm::PageSize, vm::VirtualToPhysical(code),
            vm::Page_P | vm::Page_RW | vm::Page_US);
        tf.rip = initCodeBase;
    }

    void FreeMappings(VMSpace& vs)
    {
        for(auto& m: vs.mappings) {
            if (m.inode != nullptr)
                fs::iput(*m.inode);
            for(auto& p: m.pages) {
                page_allocator::Free(p.page);
                MapMemory(vs, p.va, vm::PageSize, 0, 0);
            }
        }
        vs.mappings.clear();
    }

    void CloneMappings(VMSpace& destVS)
    {
        auto& sourceVS = GetCurrent();
        for(const auto& sourceMapping: sourceVS.mappings) {
            destVS.mappings.push_back(sourceMapping);
            auto& destMapping = destVS.mappings.back();
            destMapping = sourceMapping;
            if (destMapping.inode) fs::iref(*destMapping.inode);
            destMapping.pages.clear();

            for(const auto& p: sourceMapping.pages) {
                ClonePage(destVS, destMapping, p);
            }
        }
    }

    long VmOp(amd64::TrapFrame& tf)
    {
        auto vmopArg = syscall::GetArgument<1, VMOP_OPTIONS*>(tf);
        auto vmop = *vmopArg;
        if (!vmop) { return -EFAULT; }

        auto& vs = GetCurrent();
        switch (vmop->vo_op) {
            case OP_MAP: {
                // XXX no inode-based mappings just yet
                if ((vmop->vo_flags & (VMOP_FLAG_PRIVATE | VMOP_FLAG_FD | VMOP_FLAG_FIXED)) !=
                    VMOP_FLAG_PRIVATE)
                    return -EINVAL;

                // TODO search/overwrite mappings etc
                auto& mapping = Map(vs, vs.nextMmapAddress, ConvertVmopFlags(vmop->vo_flags), vmop->vo_len);
                vs.nextMmapAddress = mapping.va_end;

                vmop->vo_addr = reinterpret_cast<void*>(mapping.va_start);
                if (!vmopArg.Set(*vmop)) return -EFAULT;
                return 0;
            }
            case OP_UNMAP: {
                auto va = reinterpret_cast<uint64_t>(vmop->vo_addr);
                if (va < vm::userland::mmapBase)
                    return -EINVAL;
                if (va >= vs.nextMmapAddress)
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
        auto& vs = GetCurrent();
        return HandleMappingPageFault(vs, va);
    }

    Mapping& Map(VMSpace& vs, uint64_t va, uint64_t pteFlags, uint64_t mappingSize)
    {
        Mapping mapping;
        mapping.va_start = va;
        mapping.va_end = va + mappingSize;
        mapping.pte_flags = pteFlags;
        vs.mappings.push_back(mapping);
        return vs.mappings.back();
    }

    Mapping& MapInode(VMSpace& vs, uint64_t va, uint64_t pteFlags, uint64_t mappingSize, fs::Inode& inode, uint64_t inodeOffset, uint64_t inodeSize)
    {
        auto& mapping = Map(vs, va, pteFlags, mappingSize);
        mapping.inode = &inode;
        mapping.inode_offset = inodeOffset;
        mapping.inode_length = inodeSize;
        fs::iref(*mapping.inode);
        return mapping;
    }

} // namespace vm
