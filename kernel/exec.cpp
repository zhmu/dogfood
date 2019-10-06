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

#define EXEC_DEBUG 0

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

            const auto va = vm::RoundDownToPage(phdr.p_vaddr);
            const auto fileOffset = vm::RoundDownToPage(phdr.p_offset);
            auto fileSz = phdr.p_filesz + (phdr.p_offset - fileOffset);
            for (uint64_t offset = 0; offset < phdr.p_memsz; offset += vm::PageSize) {
                void* page = page_allocator::Allocate();
                if (page == nullptr)
                    return false;
                memset(page, 0, vm::PageSize);
                vm::Map(
                    pml4, va + offset, vm::PageSize,
                    vm::VirtualToPhysical(page), pteFlags);

                const auto readOffset = offset;
                int bytesToRead = vm::PageSize;
                if (readOffset + bytesToRead > fileSz)
                    bytesToRead = fileSz - readOffset;
#if EXEC_DEBUG
                printf("reading: offset %x, %d bytes -> %p\n", fileOffset + readOffset, bytesToRead, va + offset);
#endif
                if (bytesToRead > 0 &&
                    fs::Read(inode, page, fileOffset + readOffset, bytesToRead) != bytesToRead)
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
    amd64::write_cr3(current.pageDirectory);

    tf.rip = ehdr.e_entry;
    tf.rsp = vm::userland::stackBase + vm::PageSize;
    tf.rdi = vm::userland::stackBase;

    fs::iput(*inode);
    return 0;
}
