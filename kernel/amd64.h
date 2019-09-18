#pragma once

#include "types.h"

namespace amd64
{
    inline constexpr unsigned int PageSize = 4096;
    inline constexpr uint64_t KernelBase = 0xffffffff80000000;

    enum class Selector {
        KernelCode = 0x08,
        KernelData = 0x10,
        UserData = 0x18,
        UserCode = 0x20,
        Task = 0x28,
    };

    enum class DescriptorPrivilege { Supervisor = 0, User = 3 };

    enum class DescriptorContent { Code = (1 << 1) | (1 << 3), Data = (0 << 0) };

    enum class IDTType {
        InterruptGate = 0xe, // disables interrupts
        TrapGate = 0xf,      // enables interrupts
    };

    enum class IST { IST_0 = 0, IST_1 = 1 };

    struct IDTEntry {
        constexpr IDTEntry() = default;
        constexpr IDTEntry(
            const IDTType type, const IST ist, const DescriptorPrivilege dpl,
            const uint64_t handler)
            : offset_15_0(handler & 0xffff), selector(static_cast<int>(Selector::KernelCode)),
              flags(
                  static_cast<int>(ist) | (static_cast<int>(type) << 8) | (1 << 15) |
                  (static_cast<int>(dpl) << 13)),
              offset_31_16((handler >> 16) & 0xffff), offset_63_32((handler >> 32) & 0xffffffff)
        {
        }

        uint16_t offset_15_0{};
        uint16_t selector{};
        uint16_t flags{};
        uint16_t offset_31_16{};
        uint32_t offset_63_32{};
        uint32_t reserved{};
    } __attribute__((packed));
    static_assert(sizeof(IDTEntry) == 16);

    // Details a 64-bit Task State Segment */
    struct TSS {
        uint32_t _reserved0;
        uint64_t rsp0;
        uint64_t rsp1;
        uint64_t rsp2;
        uint64_t _reserved1;
        uint64_t ist1;
        uint64_t ist2;
        uint64_t ist3;
        uint64_t ist4;
        uint64_t ist5;
        uint64_t ist6;
        uint64_t ist7;
        uint64_t _reserved2;
        uint32_t _reserved3;
        uint16_t _reserved4;
        uint16_t iomap_base;
    } __attribute__((packed));
    static_assert(sizeof(TSS) == 108);

    namespace GDT
    {
        // Sets up a user/data entry in the GDT - these occupy 8 bytes
        void SetEntry64(
            uint8_t* gdt, const Selector sel, const DescriptorPrivilege dpl,
            const DescriptorContent content)
        {
            uint8_t* p = gdt + static_cast<int>(sel);
            // Segment Limit 0:15 (ignored)
            p[0] = 0;
            p[1] = 0;
            // Base Address 0:15 (ignored)
            p[2] = 0;
            p[3] = 0;
            // Base Address 16:23 (ignored)
            p[4] = 0;
            // Writable 9 [1], Conforming 10, Code 11, Must be set 12, DPL 13:14 (ignored), Present
            // 15
            p[5] = static_cast<int>(content) | (1 << 4) | (static_cast<int>(dpl) << 5) | (1 << 7);
            // Segment limit 16:19, AVL 20, Long 21, D/B 22, Granulatity 23 (all ignored)
            p[6] = (1 << 5);
            // Base address 24:31 (ignored)
            p[7] = 0;
        }

        // Sets up a GDT entry for a TSS. Note that this entry takes up 16 bytes.
        void SetTSS64(
            uint8_t* gdt, const Selector sel, const DescriptorPrivilege dpl, uint64_t base,
            uint16_t size)
        {
            uint8_t* p = gdt + static_cast<int>(sel);
            // Segment Limit 0:15
            p[0] = size & 0xff;
            p[1] = (size >> 8) & 0xff;
            // Base Address 0:15
            p[2] = base & 0xff;
            p[3] = (base >> 8) & 0xff;
            // Base Address 16:23
            p[4] = (base >> 16) & 0xff;
            // Type 8:11, DPL 13:14, Present 15
            p[5] = 9 | (static_cast<int>(dpl) << 5) | (1 << 7);
            // Segment Limit 16:19, Available 20, Granularity 23
            p[6] = (size >> 16) & 0x7;
            // Base Address 24:31
            p[7] = (base >> 24) & 0xff;
            // Base Address 32:63
            p[8] = (base >> 32) & 0xff;
            p[9] = (base >> 40) & 0xff;
            p[10] = (base >> 48) & 0xff;
            p[11] = (base >> 56) & 0xff;
            // Reserved
            p[12] = 0;
            p[13] = 0;
            p[14] = 0;
            p[15] = 0;
        }

    } // namespace GDT

    struct RRegister {
        constexpr RRegister(const uint64_t addr, const uint16_t size) : size{size}, addr{addr} {}

        const uint16_t size;
        const uint64_t addr;
    } __attribute__((packed));

    // Note: must match SAVE_REGISTERS in exception.S
    struct TrapFrame {
        /* 00 */ uint64_t trapno;
        // Stored by SAVE_REGISTERS
        /* 08 */ uint64_t rax;
        /* 10 */ uint64_t rbx;
        /* 18 */ uint64_t rcx;
        /* 20 */ uint64_t rdx;
        /* 28 */ uint64_t rbp;
        /* 30 */ uint64_t rsi;
        /* 38 */ uint64_t rdi;
        /* 40 */ uint64_t r8;
        /* 48 */ uint64_t r9;
        /* 50 */ uint64_t r10;
        /* 58 */ uint64_t r11;
        /* 60 */ uint64_t r12;
        /* 68 */ uint64_t r13;
        /* 70 */ uint64_t r14;
        /* 78 */ uint64_t r15;
        // Set by the hardware
        /* 80 */ uint64_t errnum;
        /* 88 */ uint64_t rip;
        /* 90 */ uint64_t cs;
        /* 98 */ uint64_t rflags;
        /* a0 */ uint64_t rsp;
        /* a8 */ uint64_t ss;
    } __attribute__((packed));

} // namespace amd64
