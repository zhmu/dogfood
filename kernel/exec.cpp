#include "exec.h"
#include "x86_64/amd64.h"
#include "elf.h"
#include "fs.h"
#include "lib.h"
#include "page_allocator.h"
#include "process.h"
#include "syscall.h"
#include "vm.h"
#include <dogfood/errno.h>

namespace
{
    inline constexpr auto DEBUG_EXEC = false;

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
        process::FreeMappings(current);

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

            if constexpr (DEBUG_EXEC) {
                printf(
                    "phdr %d: type %d offset %lx vaddr %p memsz %d filesz %d flags %x\n", ph,
                    phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_memsz, phdr.p_filesz,
                    phdr.p_flags);
            }
            const auto pteFlags = MapElfFlagsToVM(phdr.p_flags);
            const auto va = vm::RoundDownToPage(phdr.p_vaddr);
            const auto fileOffset = vm::RoundDownToPage(phdr.p_offset);
            const auto fileSz = phdr.p_filesz + (phdr.p_offset - fileOffset);
            if (!MapInode(current, va, pteFlags, phdr.p_memsz, inode, fileOffset, fileSz))
                return false;

        }

        return true;
    }

    template<typename Func>
    void ApplyToArgumentArray(const char** p, Func apply)
    {
        while (true) {
            apply(*p);
            if (*p == nullptr)
                break;
            ++p;
        }
    }

    void CopyArgumentContentsToStack(
        const char** args, const char* ustack, uint64_t*& sp, char*& data_sp)
    {
        ApplyToArgumentArray(args, [&](auto p) {
            size_t len;
            uint64_t ptr;
            if (p != nullptr) {
                len = strlen(p) + 1;
                ptr = vm::userland::stackBase + vm::userland::stackSize - vm::PageSize +
                      (data_sp - ustack);
            } else {
                len = 0;
                ptr = 0;
            }
            *sp++ = ptr;
            memcpy(data_sp, p, len);
            data_sp += len;
        });
    }

    void* PrepareNewUserlandStack(process::Process& proc, const char** argv, const char** envp)
    {
        auto ustack = reinterpret_cast<char*>(page_allocator::Allocate());
        assert(ustack != nullptr);
        memset(ustack, 0, vm::PageSize);

        int argc = 0, envc = 0;
        ApplyToArgumentArray(argv, [&](auto p) { ++argc; });
        ApplyToArgumentArray(envp, [&](auto p) { ++envc; });

        auto sp = reinterpret_cast<uint64_t*>(ustack);
        *sp++ = argc - 1; /* do not count nullptr */
        auto data_sp = reinterpret_cast<char*>(sp + argc + envc);
        CopyArgumentContentsToStack(argv, ustack, sp, data_sp);
        CopyArgumentContentsToStack(envp, ustack, sp, data_sp);
        return ustack;
    }
} // namespace

int exec(amd64::TrapFrame& tf)
{
    const auto path = syscall::GetArgument<1, const char*>(tf);
    const auto argv = syscall::GetArgument<2, const char**>(tf);
    const auto envp = syscall::GetArgument<3, const char**>(tf);
    auto inode = fs::namei(path, true);
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

    // We prepare the new userland stack before loading the ELF as that will
    // free out mappings
    auto ustack = PrepareNewUserlandStack(process::GetCurrent(), argv, envp);
    if (ustack == nullptr) {
        fs::iput(*inode);
        return -EFAULT; // XXX
    }

    if (!LoadProgramHeaders(*inode, ehdr)) {
        page_allocator::Free(ustack);
        fs::iput(*inode);
        return -EIO;
    }

    auto& current = process::GetCurrent();
    {
        auto pd = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current.pageDirectory));
        vm::Map(
            pd, vm::userland::stackBase + vm::userland::stackSize - vm::PageSize, vm::PageSize,
            vm::VirtualToPhysical(ustack), vm::Page_P | vm::Page_RW | vm::Page_US);
        for (uint64_t va = vm::userland::stackBase;
             va < vm::userland::stackBase + vm::userland::stackSize - vm::PageSize;
             va += vm::PageSize) {
            vm::Map(pd, va, vm::PageSize, 0, vm::Page_RW | vm::Page_US);
        }
    }
    amd64::write_cr3(current.pageDirectory);

    tf.rip = ehdr.e_entry;
    tf.rsp = vm::userland::stackBase + vm::userland::stackSize - vm::PageSize;
    tf.rdi = vm::userland::stackBase + vm::userland::stackSize - vm::PageSize;

    fs::iput(*inode);
    return 0;
}
