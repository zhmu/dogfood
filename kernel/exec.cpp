#include "exec.h"
#include "amd64.h"
#include "elf.h"
#include "errno.h"
#include "fs.h"
#include "lib.h"
#include "page_allocator.h"
#include "process.h"
#include "syscall.h"
#include "vm.h"

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

    uint64_t MapElfFlagsToVM(const int flags)
    {
        uint64_t result = vm::Page_P | vm::Page_US;
        if ((flags & PF_X) == 0)
            result |= vm::Page_NX;
        if (flags & PF_W)
            result |= vm::Page_RW;
        return result;
    }

    bool LoadProgramHeaders(fs::Inode& inode, const Elf64_Ehdr& ehdr)
    {
        auto& current = process::GetCurrent();
        {
            auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
            vm::FreeUserlandPageDirectory(pml4);
        }

        auto pml4 = vm::CreateUserlandPageDirectory();
        current.pageDirectory = vm::VirtualToPhysical(pml4);
        for (int ph = 0; ph < ehdr.e_phnum; ++ph) {
            Elf64_Phdr phdr;
            if (fs::Read(
                    inode, reinterpret_cast<void*>(&phdr), ehdr.e_phoff + ph * sizeof(phdr),
                    sizeof(phdr)) != sizeof(phdr)) {
                return false;
            }
            if (phdr.p_type != PT_LOAD)
                continue;

#if EXEC_DEBUG
            printf(
                "phdr %d: type %d offset %lx vaddr %p memsz %d filesz %d flags %x\n", ph,
                phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_memsz, phdr.p_filesz,
                phdr.p_flags);
#endif
            const auto pteFlags = MapElfFlagsToVM(phdr.p_flags);
            const auto totalPages = vm::RoundUpToPage(phdr.p_memsz) / vm::PageSize;
            for (int n = 0; n < totalPages; ++n) {
                void* page = page_allocator::Allocate();
                if (page == nullptr)
                    return false;
                memset(page, 0, vm::PageSize);
#if EXEC_DEBUG
                printf(
                    "map: va %p pa %p flags %lx\n", phdr.p_vaddr + n * vm::PageSize,
                    vm::VirtualToPhysical(page), pteFlags);
#endif

                vm::Map(
                    pml4, phdr.p_vaddr + n * vm::PageSize, vm::PageSize,
                    vm::VirtualToPhysical(page), pteFlags);

                const auto readOffset = n * vm::PageSize;
                int bytesToRead = vm::PageSize;
                if (readOffset + bytesToRead > phdr.p_filesz)
                    bytesToRead = phdr.p_filesz - readOffset;
                if (bytesToRead > 0 &&
                    fs::Read(inode, page, phdr.p_offset + readOffset, bytesToRead) != bytesToRead)
                    return false;
            }
        }

        return true;
    }
} // namespace

int exec(amd64::TrapFrame& tf)
{
    const auto path = reinterpret_cast<const char*>(syscall::GetArgument<1>(tf));
    const auto argv = reinterpret_cast<const char**>(syscall::GetArgument<2>(tf));
    const auto envp = reinterpret_cast<const char**>(syscall::GetArgument<3>(tf));
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
    if (!LoadProgramHeaders(*inode, ehdr)) {
        fs::iput(*inode);
        return -EIO;
    }

    auto& current = process::GetCurrent();
    auto ustack = CreateAndMapUserStack(current) + vm::PageSize;

    amd64::FlushTLB();
    tf.rip = ehdr.e_entry;
    tf.rsp = vm::userland::stackBase + vm::PageSize;

    fs::iput(*inode);
    return 0;
}
