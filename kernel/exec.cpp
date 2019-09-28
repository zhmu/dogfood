#include "exec.h"
#include "amd64.h"
#include "elf.h"
#include "errno.h"
#include "fs.h"
#include "lib.h"
#include "page_allocator.h"
#include "process.h"
#include "vm.h"

int koe()
{
    static volatile int x = 0;
    return ++x;
}

namespace
{
    bool VerifyHeader(const Elf64_Ehdr& ehdr)
    {
        if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
            ehdr.e_ident[EI_CLASS] != ELFCLASS64 || ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
            return false;
        if (ehdr.e_type != ET_EXEC)
            return false;
        if (ehdr.e_machine != EM_X86_64)
            return false;
        if (ehdr.e_version != EV_CURRENT)
            return false;
        return true;
    }
} // namespace

int exec(amd64::Syscall& sc)
{
    const auto path = reinterpret_cast<const char*>(sc.arg1);
    const auto argv = reinterpret_cast<const char**>(sc.arg2);
    const auto envp = reinterpret_cast<const char**>(sc.arg3);
    auto inode = fs::namei(path);
    if (inode == nullptr)
        return -ENOENT;

    Elf64_Ehdr ehdr;
    if (fs::Read(*inode, reinterpret_cast<void*>(&ehdr), 0, sizeof(ehdr)) != sizeof(ehdr)) {
        fs::iput(*inode);
        return -EIO;
    }

    if (!VerifyHeader(ehdr)) {
        fs::iput(*inode);
        return -ENOEXEC;
    }
    printf("ehdr: entry %p \n", ehdr.e_entry);

    auto& current = process::GetCurrent();
    auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
    for (int ph = 0; ph < ehdr.e_phnum; ++ph) {
        Elf64_Phdr phdr;
        if (fs::Read(
                *inode, reinterpret_cast<void*>(&phdr), ehdr.e_phoff + ph * sizeof(phdr),
                sizeof(phdr)) != sizeof(phdr)) {
            fs::iput(*inode);
            return -EIO;
        }
        if (phdr.p_type != PT_LOAD)
            continue;

        printf(
            "phdr %d: type %d offset %lx vaddr %p memsz %d filesz %d flags %x\n", ph, phdr.p_type,
            phdr.p_offset, phdr.p_vaddr, phdr.p_memsz, phdr.p_filesz, phdr.p_flags);

        auto pteFlags = vm::Page_P | vm::Page_US;
        if ((phdr.p_flags & PF_X) == 0)
            pteFlags |= vm::Page_NX;
        if (phdr.p_flags & PF_W)
            pteFlags |= vm::Page_RW;

        auto totalPages = vm::RoundUpToPage(phdr.p_memsz) / vm::PageSize;
        for (int n = 0; n < totalPages; ++n) {
            void* page = page_allocator::Allocate();
            assert(page != nullptr);
            memset(page, 0, vm::PageSize);
            printf(
                "map: va %p pa %p flags %lx\n", phdr.p_vaddr + n * vm::PageSize,
                vm::VirtualToPhysical(page), pteFlags);
            vm::Map(
                pml4, phdr.p_vaddr + n * vm::PageSize, vm::PageSize, vm::VirtualToPhysical(page),
                pteFlags);

            auto r = fs::Read(*inode, page, phdr.p_offset, vm::PageSize);
            printf("r %lx\n", r);
            if (r <= 0) {
                fs::iput(*inode);
                return -EIO;
            }
        }
    }

    __asm __volatile("movq %cr3, %rax; movq %rax, %cr3");
    koe();
    printf("all loaded!\n");
    sc.rip = ehdr.e_entry;

    fs::iput(*inode);
    return -1;
}
