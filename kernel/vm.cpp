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

        char* AllocateMDPage(VMSpace& vs)
        {
            auto new_page = page_allocator::AllocateOne();
            assert(new_page);

            auto ptr = new_page->GetData();
            vs.mdPages.push_back(std::move(new_page));
            memset(ptr, 0, vm::PageSize);
            return reinterpret_cast<char*>(ptr);
        }

        bool HandleMappingPageFault(VMSpace& vs, const uint64_t virt)
        {
            const auto va = RoundDownToPage(virt);
            for(auto& mapping: vs.mappings) {
                if (va < mapping.va_start || va >= mapping.va_end) continue;
                auto page = page_allocator::AllocateOne();
                if (!page)
                    return false;
                memset(page->GetData(), 0, vm::PageSize);

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
                    fs::Read(*mapping.inode, page->GetData(), mapping.inode_offset + readOffset, bytesToRead) != bytesToRead) {
                    return false;
                }

                const auto pagePhysicalAddr = page->GetPhysicalAddress();
                mapping.pages.push_back(MappedPage{va, std::move(page) });

                MapMemory(vs, va, vm::PageSize, pagePhysicalAddr, mapping.pte_flags);
                return true;
            }
            return false;
        }

        bool CanReusePage(const Mapping& mapping, const MappedPage& mp)
        {
            if ((mapping.pte_flags & Page_RW) == 0)
                return true;
            return false;
        }

        void CloneMappedPage(VMSpace& vs, Mapping& destMapping, MappedPage& mp)
        {
            page_allocator::PageRef newPage;
            if (CanReusePage(destMapping, mp)) {
                newPage = page_allocator::AddReference(mp.page);
            } else {
                newPage = page_allocator::AllocateOne();
                assert(newPage);
                memcpy(newPage->GetData(), mp.page->GetData(), vm::PageSize);
            }
            MapMemory(vs, mp.va, vm::PageSize, newPage->GetPhysicalAddress(), destMapping.pte_flags);
            destMapping.pages.push_back({ mp.va, std::move(newPage) });
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

    void Dump(VMSpace& vs)
    {
        using namespace print;
        for(const auto& m: vs.mappings) {
            Print("  area ", Hex{m.va_start}, " .. ", Hex{m.va_end}, "\n");
            for(auto& mp: m.pages) {
                Print("    va ", Hex{mp.va}, " page ", mp.page.get(), " refcount ", mp.page->refcount.load(), "\n");
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
                const auto new_page = AllocateMDPage(vs);
                return vm::VirtualToPhysical(new_page) | Page_P | Page_US | Page_RW;
            });
            assert(pte != nullptr);
            *pte = phys | pteFlags;
            pa += PageSize;
            va += PageSize;
        } while (va < va_end);
    }

    void Activate(VMSpace& vs)
    {
        amd64::write_cr3(vs.pageDirectory);
    }

    void InitializeVMSpace(VMSpace& vs)
    {
        assert(vs.pageDirectory == 0);
        assert(vs.mdPages.empty());

        // Allocate kernel stack
        auto kstack = AllocateMDPage(vs);
        vs.kernelStack = kstack;

        // Create page directory
        auto pageDirectory = AllocateMDPage(vs);
        memcpy(pageDirectory, kernel_pagedir, vm::PageSize);
        vs.pageDirectory = vm::VirtualToPhysical(pageDirectory);

        vs.nextMmapAddress = vm::userland::mmapBase;
    }

    void DestroyVMSpace(VMSpace& vs)
    {
        assert(!IsActive(vs));
        assert(vs.mappings.empty());
        vs.mdPages.clear(); // will Deref() all of them
        vs.pageDirectory = 0;
    }

    void SetupForInitProcess(VMSpace& vs, amd64::TrapFrame& tf)
    {
        // Set up userland stack; it will be paged in as needed
        Map(vs, vm::userland::stackBase, vm::Page_P | vm::Page_RW | vm::Page_US, vm::PageSize);
        tf.rsp = vm::userland::stackBase + vm::PageSize;

        // Fill a page with code to execve("/sbin/init", ...)
        auto code = AllocateMDPage(vs);
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
            for(auto& mp: m.pages) {
                MapMemory(vs, mp.va, vm::PageSize, 0, 0);
            }
        }
        vs.mappings.clear();
    }

    void Clone(VMSpace& destVS)
    {
        auto& sourceVS = GetCurrent();
        for(auto& sourceMapping: sourceVS.mappings) {
            destVS.mappings.push_back(Mapping{
                sourceMapping.pte_flags,
                sourceMapping.va_start,
                sourceMapping.va_end,
                sourceMapping.inode,
                sourceMapping.inode_offset,
                sourceMapping.inode_length,
            });
            auto& destMapping = destVS.mappings.back();
            if (destMapping.inode) fs::iref(*destMapping.inode);

            for(auto& p: sourceMapping.pages) {
                CloneMappedPage(destVS, destMapping, p);
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
        vs.mappings.push_back(Mapping{ pteFlags, va, va + mappingSize });
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
