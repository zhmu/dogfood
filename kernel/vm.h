#pragma once

#include <atomic>
#include <vector>
#include "page_allocator.h"
#include "result.h"
#include "types.h"

namespace amd64
{
    using PageDirectory = uint64_t*;

    struct TrapFrame;
}

namespace fs { struct Inode; }
namespace page_allocator { struct Page; }

namespace vm
{
    inline constexpr unsigned int PageSize = 4096;

    inline constexpr uint64_t Page_P = (1UL << 0);
    inline constexpr uint64_t Page_RW = (1UL << 1); // 1 = r/w, 0 = r/0
    inline constexpr uint64_t Page_US = (1UL << 2); // 1 = u/s, 0 = s
    inline constexpr uint64_t Page_G = (1UL << 8);
    inline constexpr uint64_t Page_NX = (1UL << 63);

    namespace userland
    {
        inline constexpr uint64_t stackBase = 0x10000;
        inline constexpr uint64_t stackSize = 1024 * 1024;
        inline constexpr uint64_t mmapBase = 0x0000008000000000;
    } // namespace userland

    struct MappedPage {
        const uint64_t va{};
        page_allocator::PageRef page;
    };

    struct Mapping {
        uint64_t pte_flags{};
        uint64_t va_start{};
        uint64_t va_end{};
        fs::Inode* inode = nullptr;
        uint64_t inode_offset{};
        uint64_t inode_length{};
        std::vector<MappedPage> pages;
    };

    struct VMSpace {
        uint64_t pageDirectory = 0;  // physical address
        uint64_t nextMmapAddress = 0;
        void* kernelStack = nullptr; // start of kernel stack
        std::vector<Mapping> mappings;
        std::vector<page_allocator::PageRef> mdPages; // machine-dependant pages
    };

    void
    MapMemory(VMSpace&, const uint64_t va_start, const size_t length, const uint64_t phys,
        const uint64_t pteFlags);

    void InitializeVMSpace(VMSpace&);
    void DestroyVMSpace(VMSpace&);
    void Activate(VMSpace&);

    void SetupForInitProcess(VMSpace& vs, amd64::TrapFrame& tf);

    Mapping& Map(VMSpace& vs, uint64_t va, uint64_t pteFlags, uint64_t mappingSize);
    Mapping& MapInode(VMSpace& vs, uint64_t va, uint64_t pteFlags, uint64_t mappingSize, fs::Inode& inode, uint64_t inodeOffset, uint64_t inodeSize);
    void FreeMappings(VMSpace&);
    void Clone(VMSpace&);

    result::MaybeInt VmOp(amd64::TrapFrame& tf);
    bool HandlePageFault(uint64_t va, int errnum);

    void Dump(VMSpace&);

    /*
     * We use the following memory map, [G] means global mapped:
     *
     * From                  To                       Type               Size
     * 0000 0000 0000 0000 - 0000 7fff ffff ffff      Application        127TB
     * ffff 8800 0000 0000 - ffff c7ff ffff ffff  [G] Direct mappings    64TB
     * ffff ffff 8000 0000 - ffff ffff ffff ffff  [G] Kernel text/data   2GB
     */

    /* Convert a physical to a kernel virtual address */
    template<typename Address>
    constexpr uint64_t PhysicalToVirtual(Address addr)
    {
        return reinterpret_cast<uint64_t>(addr) | 0xffff880000000000;
    }

    template<typename Address>
    constexpr uint64_t VirtualToPhysical(Address addr)
    {
        return reinterpret_cast<uint64_t>(addr) & ~0xffff880000000000;
    }

    template<typename T>
    uint64_t RoundDownToPage(T v)
    {
        auto addr = reinterpret_cast<uint64_t>(v);
        addr &= ~(static_cast<uint64_t>(PageSize) - 1);
        return addr;
    }

    template<typename T>
    uint64_t RoundUpToPage(T v)
    {
        auto addr = reinterpret_cast<uint64_t>(v);
        addr = (addr | (PageSize - 1)) + 1;
        return addr;
    }
} // namespace vm
