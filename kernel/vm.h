#pragma once

#include "types.h"

namespace amd64
{
    using PageDirectory = uint64_t*;

    struct TrapFrame;
}

namespace fs { struct Inode; }
namespace process { struct Process; struct Mapping; }


extern amd64::PageDirectory kernel_pagedir;

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

    void
    MapMemory(process::Process&, const uint64_t va_start, const size_t length, const uint64_t phys,
        const uint64_t pteFlags);

    uint64_t* CreateUserlandPageDirectory(process::Process&);
    void DestroyUserlandPageDirectory(process::Process& proc);
    char* CreateAndMapUserStack(process::Process& proc);

    process::Mapping& Map(process::Process& proc, uint64_t va, uint64_t pteFlags, uint64_t mappingSize);
    process::Mapping& MapInode(process::Process& proc, uint64_t va, uint64_t pteFlags, uint64_t mappingSize, fs::Inode& inode, uint64_t inodeOffset, uint64_t inodeSize);
    void FreeMappings(process::Process& proc);
    void CloneMappings(process::Process&);

    long VmOp(amd64::TrapFrame& tf);
    bool HandlePageFault(uint64_t va, int errnum);

    void Dump(process::Process&);

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
