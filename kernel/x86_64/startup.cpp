#include "lib.h"
#include "multiboot.h"
#include "page_allocator.h"
#include "amd64.h"
#include "bio.h"
#include "ext2.h"
#include "fs.h"
#include "process.h"
#include "vm.h"

#include "hw/console.h"
#include "hw/ide.h"
#include "hw/pic.h"

using namespace amd64;

TSS kernel_tss;
uint64_t* kernel_pagedir = nullptr;

inline constexpr int gdtSize = static_cast<int>(Selector::Task) + 16;
uint8_t gdt[gdtSize];

inline constexpr int numberOfIDTEntries = 256;
IDTEntry idt[numberOfIDTEntries];

extern "C" void exception0();
extern "C" void exception1();
extern "C" void exception2();
extern "C" void exception3();
extern "C" void exception4();
extern "C" void exception5();
extern "C" void exception6();
extern "C" void exception7();
extern "C" void exception8();
extern "C" void exception9();
extern "C" void exception10();
extern "C" void exception11();
extern "C" void exception12();
extern "C" void exception13();
extern "C" void exception14();
extern "C" void exception16();
extern "C" void exception17();
extern "C" void exception18();
extern "C" void exception19();

extern "C" void irq0();
extern "C" void irq1();
extern "C" void irq2();
extern "C" void irq3();
extern "C" void irq4();
extern "C" void irq5();
extern "C" void irq6();
extern "C" void irq7();
extern "C" void irq8();
extern "C" void irq9();
extern "C" void irq10();
extern "C" void irq11();
extern "C" void irq12();
extern "C" void irq13();
extern "C" void irq14();
extern "C" void irq15();
extern "C" void syscall_handler();

extern "C" void* bootstrap_stack;
extern "C" void* __entry;
extern "C" void* __rodata_end;
extern "C" void* __rwdata_begin;
extern "C" void* __rwdata_end;
extern "C" void* __bss_begin;
extern "C" void* __bss_end;
extern "C" void* __end;

namespace
{
    void SetupDescriptors()
    {
        using namespace amd64;
        GDT::SetEntry64(
            gdt, Selector::KernelCode, DescriptorPrivilege::Supervisor, DescriptorContent::Code);
        GDT::SetEntry64(
            gdt, Selector::KernelData, DescriptorPrivilege::Supervisor, DescriptorContent::Data);
        GDT::SetEntry64(
            gdt, Selector::UserCode, DescriptorPrivilege::User, DescriptorContent::Code);
        GDT::SetEntry64(
            gdt, Selector::UserData, DescriptorPrivilege::User, DescriptorContent::Data);
        GDT::SetTSS64(
            gdt, Selector::Task, DescriptorPrivilege::Supervisor,
            reinterpret_cast<uint64_t>(&kernel_tss), sizeof(kernel_tss));

        memset(&kernel_tss, 0, sizeof(kernel_tss));
        kernel_tss.ist1 = reinterpret_cast<uint64_t>(&bootstrap_stack);

        // Load new GDT and reload all descriptors
        {
            volatile const RRegister gdtr{reinterpret_cast<uint64_t>(&gdt), gdtSize - 1};
            __asm __volatile("lgdt (%%rax)\n"
                             "mov %%cx, %%ds\n"
                             "mov %%cx, %%es\n"
                             "mov %%cx, %%fs\n"
                             "mov %%cx, %%gs\n"
                             "mov %%cx, %%ss\n"
                             "ltr %%dx\n"
                             "pushq %%rbx\n"
                             "pushq $1f\n"
                             "lretq\n"
                             "1:\n"
                             :
                             : "a"(&gdtr), "b"(static_cast<int>(Selector::KernelCode)),
                               "c"(static_cast<int>(Selector::KernelData)),
                               "d"(static_cast<int>(Selector::Task)));
        }

        idt[0] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception0)};
        idt[1] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception1)};
        idt[2] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception2)};
        idt[3] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception3)};
        idt[4] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception4)};
        idt[5] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception5)};
        idt[6] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception6)};
        idt[7] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception7)};
        // Use interrupt stack 1 for double fault
        idt[8] = IDTEntry{IDTType::InterruptGate, IST::IST_1, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception8)};
        idt[9] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                          reinterpret_cast<uint64_t>(&exception9)};
        idt[10] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception10)};
        idt[11] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception11)};
        idt[12] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception12)};
        idt[13] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception13)};
        idt[14] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception14)};
        // There is no exception 15 ... ?
        idt[16] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception16)};
        idt[17] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception17)};
        idt[18] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception18)};
        idt[19] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&exception19)};

        idt[32] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq0)};
        idt[33] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq1)};
        idt[34] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq2)};
        idt[35] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq3)};
        idt[36] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq4)};
        idt[37] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq5)};
        idt[38] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq6)};
        idt[39] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq7)};
        idt[40] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq8)};
        idt[41] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq9)};
        idt[42] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq10)};
        idt[43] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq11)};
        idt[44] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq12)};
        idt[45] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq13)};
        idt[46] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq14)};
        idt[47] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
                           reinterpret_cast<uint64_t>(&irq15)};

        volatile const RRegister idtr{reinterpret_cast<uint64_t>(&idt),
                                      (numberOfIDTEntries * 16) - 1};
        __asm __volatile("lidt (%%rax)" : : "a"(&idtr));
    }

    uint64_t* GetNextPage(uint64_t& next_page)
    {
        auto ptr = reinterpret_cast<uint64_t*>(next_page);
        memset(ptr, 0, sizeof(vm::PageSize));
        next_page += vm::PageSize;
        return ptr;
    }

    uint64_t* CreateOrGetPage(uint64_t& entry, uint64_t& next_page)
    {
        if ((entry & vm::Page_P) == 0) {
            entry = reinterpret_cast<uint64_t>(GetNextPage(next_page)) | vm::Page_P | vm::Page_RW;
        }
        return reinterpret_cast<uint64_t*>(entry & 0xffffffffff000);
    };

    void MapMemoryArea(
        uint64_t* pml4, uint64_t& next_page, uint64_t phys_base, uint64_t va_start, uint64_t va_end,
        uint64_t pteFlags)
    {
        for (uint64_t addr = va_start; addr < va_end; addr += vm::PageSize) {
            const auto pml4Offset = (addr >> 39) & 0x1ff;
            const auto pdpeOffset = (addr >> 30) & 0x1ff;
            const auto pdpOffset = (addr >> 21) & 0x1ff;
            const auto pteOffset = (addr >> 12) & 0x1ff;

            auto pdpe = CreateOrGetPage(pml4[pml4Offset], next_page);
            auto pdp = CreateOrGetPage(pdpe[pdpeOffset], next_page);
            auto pte = CreateOrGetPage(pdp[pdpOffset], next_page);
            pte[pteOffset] = (addr - va_start + phys_base) | pteFlags;
        }
    }

    void InitializeMemory(const MULTIBOOT& mb)
    {
        // Determine where the kernel resides in memory - we need to
        // exclude this range from our memory map
        const uint64_t kernel_phys_start = vm::RoundDownToPage(&__entry) - KernelBase;
        const uint64_t kernel_phys_end = vm::RoundUpToPage(&__end) - KernelBase;
        printf("kernel physical memory: %lx .. %lx\n", kernel_phys_start, kernel_phys_end);

        // Convert the memory into regions
        struct Region {
            uint64_t base{};
            uint64_t length{};
        };

        constexpr int maxRegions = 16;
        int currentRegion = 0;
        Region regions[maxRegions];
        auto addRegion = [&](const auto& region) {
            if (currentRegion == maxRegions)
                return;
            regions[currentRegion] = region;
            ++currentRegion;
        };

        {
            const auto mm_end = reinterpret_cast<const char*>(mb.mb_mmap_addr + mb.mb_mmap_length);
            for (auto mm_ptr = reinterpret_cast<const char*>(mb.mb_mmap_addr); mm_ptr < mm_end;
                 /* nothing */) {
                const auto mm = reinterpret_cast<const MULTIBOOT_MMAP*>(mm_ptr);
                const auto entry_len = mm->mm_entry_len + sizeof(uint32_t);
                mm_ptr += entry_len;
                if (mm->mm_type != MULTIBOOT_MMAP_AVAIL)
                    continue;

                // Combine the multiboot mmap to a base/length pair
                const auto base = static_cast<uint64_t>(mm->mm_base_hi) << 32 | mm->mm_base_lo;
                const auto length = (static_cast<uint64_t>(mm->mm_len_hi) << 32 | mm->mm_len_lo);

                // We'll assume the region starts where the kernel resides; we'll need to
                // adjust the base if this happens
                const auto region = [&]() {
                    if (base == kernel_phys_start)
                        return Region{kernel_phys_end, (base + length) - kernel_phys_end};
                    return Region{base, length};
                }();
                addRegion(region);
            }
        }

        printf("physical memory regions:\n");
        for (unsigned int n = 0; n < currentRegion; ++n) {
            const auto& region = regions[n];
            printf("  base %lx, %ld KB\n", region.base, region.length / 1024);
        }

        // Create mappings so that we can identity map all physical memory
        auto next_page = kernel_phys_end;
        auto pml4 = GetNextPage(next_page);

        // Map all memory regions; this is read/write
        for (unsigned int n = 0; n < currentRegion; ++n) {
            const auto& region = regions[n];
            MapMemoryArea(
                pml4, next_page, region.base, vm::PhysicalToVirtual(region.base),
                vm::PhysicalToVirtual(region.base) + region.length,
                vm::Page_NX | vm::Page_G | vm::Page_RW | vm::Page_P);
        }

        // Map the kernel itself - we do this per section to honor read/only content
        auto mapKernel = [&](void* from, void* to, uint64_t pteFlags) {
            const auto start = reinterpret_cast<uint64_t>(from);
            const auto end = reinterpret_cast<uint64_t>(to);
            MapMemoryArea(
                pml4, next_page, start - KernelBase, start, end,
                vm::Page_G | vm::Page_P | pteFlags);
        };
        mapKernel(&__entry, &__rodata_end, 0);                                // code + rodata
        mapKernel(&__rwdata_begin, &__rwdata_end, vm::Page_NX | vm::Page_RW); // data
        mapKernel(&__bss_begin, &__bss_end, vm::Page_NX | vm::Page_RW);       // bss

        // Enable necessary features and use our new page tables
        wrmsr(msr::EFER, rdmsr(msr::EFER) | msr::EFER_NXE);    // No-Execute pages
        write_cr4(read_cr4() | cr4::PGE);                      // Global pages
        write_cr4(read_cr4() | cr4::OSXMMEXCPT | cr4::OSFXSR); // FPU support
        write_cr3(reinterpret_cast<uint64_t>(pml4));
        kernel_pagedir =
            reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(reinterpret_cast<uint64_t>(pml4)));

        // Register all available regions with our memory allocation now that
        // they are properly mapped.  Note that next_page is the kernel end +
        // the pages we used to store the memory mappings, so we'll need to
        // adjust the region to avoid re-using that memory.
        page_allocator::Initialize();
        for (unsigned int n = 0; n < currentRegion; ++n) {
            auto region = regions[n];
            if (region.base == kernel_phys_end) {
                region.length -= next_page - kernel_phys_end;
                region.base = next_page;
            }
            page_allocator::RegisterMemory(
                vm::PhysicalToVirtual(region.base), region.length / vm::PageSize);
        }
    }

    void InitializeSyscall()
    {
        constexpr auto star = (static_cast<uint64_t>(
                                   (static_cast<int>(Selector::UserCode) - 0x10) |
                                   static_cast<int>(DescriptorPrivilege::User))
                               << 48L) |
                              (static_cast<uint64_t>(Selector::KernelCode) << 32L);
        wrmsr(msr::STAR, star);
        wrmsr(msr::LSTAR, reinterpret_cast<uint64_t>(&syscall_handler));
        wrmsr(msr::SFMASK, 0x200); // IF
        wrmsr(msr::EFER, rdmsr(msr::EFER) | msr::EFER_SCE);
    }
} // namespace

extern "C" void exception(struct TrapFrame* tf)
{
    const bool isUserMode = (tf->cs & 3) == static_cast<int>(DescriptorPrivilege::User);
    const bool isPageFault = tf->trapno == exception::PF;

    const auto faultAddress = read_cr2();
    if (isPageFault && vm::HandlePageFault(faultAddress, tf->errnum))
        return;

    printf("exception #%d @ cs:rip = %lx:%lx\n", tf->trapno, tf->cs, tf->rip);
    printf("rax %lx rbx %lx rcx %lx rdx %lx\n", tf->rax, tf->rbx, tf->rcx, tf->rdx);
    printf("rsi %lx rdi %lx rbp %lx rsp %lx\n", tf->rsi, tf->rdi, tf->rbp, tf->rsp);
    printf("r8 %lx r9 %lx r10 %lx r11 %lx\n", tf->r8, tf->r9, tf->r10, tf->r11);
    printf("r12 %lx r13 %lx r14 %lx r15 %lx\n", tf->r12, tf->r13, tf->r14, tf->r15);
    printf(
        "errnum %lx cs %lx rflags %lx ss:esp %lx:%lx\n", tf->errnum, tf->cs, tf->rflags, tf->ss,
        tf->rsp);
    if (isPageFault)
        printf("fault address %lx\n", faultAddress);

    if (isUserMode && isUserMode)
        process::Exit(*tf);

    while (1)
        ;
}

extern "C" void irq_handler(const struct TrapFrame* tf)
{
    switch (tf->trapno) {
        case pic::irq::Timer:
            break;
        case pic::irq::COM1:
            console::OnIRQ();
            break;
        case pic::irq::IDE:
            ide::OnIRQ();
            break;
        default:
            printf("stray irq %d\n", tf->trapno);
    }
    pic::Acknowledge();
}

extern "C" void startup(const MULTIBOOT* mb)
{
    SetupDescriptors();
    console::initialize();
    pic::Initialize();
    InitializeMemory(*mb);
    InitializeSyscall();
    bio::Initialize();

    printf(
        "Dogfood/amd64 - %ld MB memory available\n",
        (page_allocator::GetNumberOfAvailablePages() * (vm::PageSize / 1024UL)) / 1024UL);

    ide::Initialize();
    pic::Enable(pic::irq::Timer);
    pic::Enable(pic::irq::COM1);
    __asm __volatile("sti");
    fs::Initialize();
    process::Initialize();

    process::Scheduler();
}
