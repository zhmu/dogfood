#include "console.h"
#include "lib.h"
#include "multiboot.h"
#include "amd64.h"

amd64::TSS kernel_tss;

inline constexpr int gdtSize = (4 * 8) + 16;
uint8_t gdt[gdtSize];

inline constexpr int numberOfIDTEntries = 256;
amd64::IDTEntry idt[numberOfIDTEntries];

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

} // namespace

extern "C" void exception(const struct amd64::TrapFrame* tf)
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

extern "C" void startup(const struct MULTIBOOT* mb)
{
    SetupDescriptors();
    console::initialize();

    printf("hello world!\n");
    printf("mb flags %x\n", mb->mb_flags);

    // Walk through the loader-provided memory map
    {
        const auto mm_end = reinterpret_cast<const char*>(mb->mb_mmap_addr + mb->mb_mmap_length);
        for (auto mm_ptr = reinterpret_cast<const char*>(mb->mb_mmap_addr); mm_ptr < mm_end;
             /* nothing */) {
            const auto mm = reinterpret_cast<const MULTIBOOT_MMAP*>(mm_ptr);
            const auto entry_len = mm->mm_entry_len + sizeof(uint32_t);
            mm_ptr += entry_len;

            printf(
                "entry base %x:%x len %x:%x type %d\n", (int)mm->mm_base_hi, (int)mm->mm_base_lo,
                (int)mm->mm_len_hi, (int)mm->mm_len_lo, (int)mm->mm_type);
        }
    }

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
