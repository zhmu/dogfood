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

    bool LoadProgramHeaders(vm::VMSpace& vs, fs::Inode& inode, const Elf64_Ehdr& ehdr)
    {
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
                Print("phdr ", ph, ": type ", phdr.p_type, " offset ",
                    print::Hex{phdr.p_offset}, " vaddr ", print::Hex{phdr.p_vaddr},
                    " memsz ", phdr.p_memsz, " filesz ", phdr.p_filesz,
                    " flags ", print::Hex{phdr.p_flags}, "\n");
            }
            const auto pteFlags = MapElfFlagsToVM(phdr.p_flags);
            const auto va = vm::RoundDownToPage(phdr.p_vaddr);
            const auto fileOffset = vm::RoundDownToPage(phdr.p_offset);
            const auto fileSz = phdr.p_filesz + (phdr.p_offset - fileOffset);
            vm::MapInode(vs, va, pteFlags, phdr.p_memsz, inode, fileOffset, fileSz);
        }
        return true;
    }

    template<typename Func>
    void ApplyToArgumentArray(const char* const* p, Func apply)
    {
        while (true) {
            apply(*p);
            if (*p == nullptr)
                break;
            ++p;
        }
    }

    void CopyArgumentContentsToStack(
        const char* const* args, const char* ustack, uint64_t*& sp, char*& data_sp)
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

    void* PrepareNewUserlandStack(const char* const* argv, const char* const* envp)
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

    void MapUserlandStack(vm::VMSpace& vs, void* ustack, amd64::TrapFrame& tf)
    {
        const auto ustackFlags = vm::Page_P | vm::Page_RW | vm::Page_US;
        auto& mapping = vm::Map(vs, vm::userland::stackBase, ustackFlags, vm::userland::stackSize);
        const auto ustackVA = vm::userland::stackBase + vm::userland::stackSize - vm::PageSize;
        mapping.pages.push_back(vm::Page{ ustackVA, ustack });

        vm::MapMemory(vs,
            ustackVA, vm::PageSize,
            vm::VirtualToPhysical(ustack), ustackFlags);

        tf.rsp = ustackVA;
        tf.rdi = tf.rsp;
    }
} // namespace

int exec(amd64::TrapFrame& tf)
{
    const auto path = syscall::GetArgument<1, const char*>(tf);
    const auto argv = syscall::GetArgument<2, const char**>(tf);
    const auto envp = syscall::GetArgument<3, const char**>(tf);
    auto inode = fs::namei(path.get(), true);
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

    // We must prepare the new userland stack with argc/argv/envp before
    // freeing mappings, as we need to read the current memory space
    auto& vs = process::GetCurrent().vmspace;
    auto ustack = PrepareNewUserlandStack(argv.get(), envp.get());
    vm::FreeMappings(vs);

    const auto phLoaded = LoadProgramHeaders(vs, *inode, ehdr);
    fs::iput(*inode);
    if (!phLoaded) {
        // TODO need to kill the process here
        return -EFAULT;
    }

    MapUserlandStack(vs, ustack, tf);
    amd64::FlushTLB();

    tf.rip = ehdr.e_entry;
    return 0;
}
