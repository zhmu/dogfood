#include "console.h"
#include "lib.h"
#include "multiboot.h"
#include "page_allocator.h"
#include "amd64.h"
#include "vm.h"

using namespace amd64;

TSS kernel_tss;

inline constexpr int gdtSize = (4 * 8) + 16;
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

extern "C" void* __entry;
extern "C" void* __rodata_end;
extern "C" void* __rwdata_begin;
extern "C" void* __rwdata_end;
extern "C" void* __bss_begin;
extern "C" void* __bss_end;
extern "C" void* __end;

namespace
{
    inline constexpr uint64_t Page_P = (1UL << 0);
    inline constexpr uint64_t Page_RW = (1UL << 1); // 1 = r/w, 0 = r/0
    inline constexpr uint64_t Page_US = (1UL << 2); // 1 = u/s, 0 = s
    inline constexpr uint64_t Page_G = (1UL << 8);
    inline constexpr uint64_t Page_NX = (1UL << 63);

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

        // Load new GDT and reload all descriptors
        {
            volatile const RRegister gdtr{reinterpret_cast<uint64_t>(&gdt), gdtSize};
            __asm __volatile("lgdt (%%rax)\n"
                             "mov %%cx, %%ds\n"
                             "mov %%cx, %%es\n"
                             "mov %%cx, %%fs\n"
                             "mov %%cx, %%gs\n"
                             "pushq %%rbx\n"
                             "pushq $1f\n"
                             "lretq\n"
                             "1:\n"
                             :
                             : "a"(&gdtr), "b"(static_cast<int>(Selector::KernelCode)),
                               "c"(static_cast<int>(Selector::KernelData)));
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
        idt[8] = IDTEntry{IDTType::InterruptGate, IST::IST_0, DescriptorPrivilege::Supervisor,
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

        volatile const RRegister idtr{reinterpret_cast<uint64_t>(&idt),
                                      (numberOfIDTEntries * 16) - 1};
        __asm __volatile("lidt (%%rax)" : : "a"(&idtr));
    }

    uint64_t* GetNextPage(uint64_t& next_page)
    {
        auto ptr = reinterpret_cast<uint64_t*>(next_page);
        memset(ptr, 0, sizeof(PageSize));
        next_page += PageSize;
        return ptr;
    }

    uint64_t* CreateOrGetPage(uint64_t& entry, uint64_t& next_page)
    {
        if ((entry & Page_P) == 0) {
            entry = reinterpret_cast<uint64_t>(GetNextPage(next_page)) | Page_P | Page_RW;
        }
        return reinterpret_cast<uint64_t*>(entry & 0xffffffffff000);
    };

    void MapMemoryArea(
        uint64_t* pml4, uint64_t& next_page, uint64_t phys_base, uint64_t va_start, uint64_t va_end,
        uint64_t pteFlags)
    {
        for (uint64_t addr = va_start; addr < va_end; addr += PageSize) {
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

    void InitializeMemory(const MULTIBOOT& mb)
    {
        // Determine where the kernel resides in memory - we need to
        // exclude this range from our memory map
        const uint64_t kernel_phys_start = RoundDownToPage(&__entry) - KernelBase;
        const uint64_t kernel_phys_end = RoundUpToPage(&__end) - KernelBase;
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
        uint64_t* pml4 = GetNextPage(next_page);

        // Map all memory regions; this is read/write
        for (unsigned int n = 0; n < currentRegion; ++n) {
            const auto& region = regions[n];
            MapMemoryArea(
                pml4, next_page, region.base, vm::PhysicalToVirtual(region.base),
                vm::PhysicalToVirtual(region.base) + region.length,
                Page_NX | Page_G | Page_RW | Page_P);
        }

        // Map the kernel itself - we do this per section to honor read/only content
        auto mapKernel = [&](void* from, void* to, uint64_t pteFlags) {
            const auto start = reinterpret_cast<uint64_t>(from);
            const auto end = reinterpret_cast<uint64_t>(to);
            MapMemoryArea(
                pml4, next_page, start - KernelBase, start, end, Page_G | Page_P | pteFlags);
        };
        mapKernel(&__entry, &__rodata_end, 0);                        // code + rodata
        mapKernel(&__rwdata_begin, &__rwdata_end, Page_NX | Page_RW); // data
        mapKernel(&__bss_begin, &__bss_end, Page_NX | Page_RW);       // bss

        // Enable necessary features and use our new page tables
        wrmsr(msr::EFER, rdmsr(msr::EFER) | msr::EFER_NXE); // No-Execute pages
        write_cr4(read_cr4() | cr4::PGE);                   // Global pages
        write_cr3(reinterpret_cast<uint64_t>(pml4));

        // Register all available regions with our memory allocation now that
        // they are properly mapped.  Note that next_page is the kernel end +
        // the pages we used to store the memory mappings, so we'll need to
        // adjust the region to avoid re-using that memory.
        for (unsigned int n = 0; n < currentRegion; ++n) {
            auto region = regions[n];
            if (region.base == kernel_phys_end) {
                region.length -= next_page - kernel_phys_end;
                region.base = next_page;
            }
            page_allocator::RegisterMemory(
                vm::PhysicalToVirtual(region.base), region.length / PageSize);
        }
    }

} // namespace

extern "C" void exception(const struct TrapFrame* tf)
{
    printf("exception #%d @ cs:rip = %lx:%lx\n", tf->trapno, tf->cs, tf->rip);
    printf("rax %lx rbx %lx rcx %lx rdx %lx\n", tf->rax, tf->rbx, tf->rcx, tf->rdx);
    printf("rsi %lx rdi %lx rbp %lx rsp %lx\n", tf->rsi, tf->rdi, tf->rbp, tf->rsp);
    printf("r8 %lx r9 %lx r10 %lx r11 %lx\n", tf->r8, tf->r9, tf->r10, tf->r11);
    printf("r12 %lx r13 %lx r14 %lx r15 %lx\n", tf->r12, tf->r13, tf->r14, tf->r15);
    printf(
        "errnum %lx cs %lx rflags %lx ss:esp %lx:%lx\n", tf->errnum, tf->cs, tf->rflags, tf->ss,
        tf->rsp);
    while (1)
        ;
}

extern "C" void startup(const MULTIBOOT* mb)
{
    memset(&__bss_begin, 0, (&__bss_end - &__bss_begin));
    SetupDescriptors();
    console::initialize();

    printf("hello world!\n");

    // Process the loader-provided memory map
    InitializeMemory(*mb);
    printf(
        "%ld MB memory available\n",
        (page_allocator::GetNumberOfAvailablePages() * (PageSize / 1024UL)) / 1024UL);

    __asm __volatile("movq $0x12, %rax\n"
                     "movq $0x34, %rbx\n"
                     "movq $0x56, %rcx\n"
                     "movq $0x78, %rdx\n"
                     "movq $0x123, %rbp\n"
                     "movq $0x234, %rsi\n"
                     "movq $0x345, %rdi\n"
                     "movq $0x800, %r8\n"
                     "movq $0x900, %r9\n"
                     "movq $0x1000, %r10\n"
                     "movq $0x1100, %r11\n"
                     "movq $0x1200, %r12\n"
                     "movq $0x1300, %r13\n"
                     "movq $0x1400, %r14\n"
                     "movq $0x1500, %r15\n"
                     "ud2\n");
    for (;;)
        ;
}
