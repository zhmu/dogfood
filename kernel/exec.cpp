#include "exec.h"
#include "x86_64/amd64.h"
#include "elf.h"
#include "fs.h"
#include "lib.h"
#include "debug.h"
#include "page_allocator.h"
#include "process.h"
#include "syscall.h"
#include "vm.h"
#include <dogfood/errno.h>
#include <algorithm>

namespace exec
{
    namespace
    {
        constexpr debug::Trace<false> Debug;

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

        bool LoadProgramHeaders(vm::VMSpace& vs, fs::InodeRef inode, const Elf64_Ehdr& ehdr)
        {
            for (int ph = 0; ph < ehdr.e_phnum; ++ph) {
                Elf64_Phdr phdr;
                if (fs::Read(
                        *inode, reinterpret_cast<void*>(&phdr), ehdr.e_phoff + ph * sizeof(phdr),
                        sizeof(phdr)) != sizeof(phdr)) {
                    return false;
                }
                if (phdr.p_type != PT_LOAD)
                    continue;

                Debug("phdr ", ph, ": type ", phdr.p_type, " offset ",
                    print::Hex{phdr.p_offset}, " vaddr ", print::Hex{phdr.p_vaddr},
                    " memsz ", phdr.p_memsz, " filesz ", phdr.p_filesz,
                    " flags ", print::Hex{phdr.p_flags}, "\n");
                const auto pteFlags = MapElfFlagsToVM(phdr.p_flags);
                const auto va = vm::RoundDownToPage(phdr.p_vaddr);
                const auto fileOffset = vm::RoundDownToPage(phdr.p_offset);
                const auto fileSz = phdr.p_filesz + (phdr.p_offset - fileOffset);
                vm::MapInode(vs, va, pteFlags, phdr.p_memsz, fs::ReferenceInode(inode), fileOffset, fileSz);
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

        page_allocator::PageRef PrepareNewUserlandStack(const char* const* argv, const char* const* envp)
        {
            int argc = 0, envc = 0;
            ApplyToArgumentArray(argv, [&](auto p) { ++argc; });
            ApplyToArgumentArray(envp, [&](auto p) { ++envc; });

            auto page = page_allocator::AllocateOne();
            assert(page);

            auto ustack = page->GetData();
            auto sp = reinterpret_cast<uint64_t*>(ustack);
            *sp++ = argc - 1; // do not count nullptr
            auto data_sp = reinterpret_cast<char*>(sp + argc + envc);
            CopyArgumentContentsToStack(argv, ustack, sp, data_sp);
            CopyArgumentContentsToStack(envp, ustack, sp, data_sp);
            return page;
        }

        void MapUserlandStack(vm::VMSpace& vs, page_allocator::PageRef&& page, amd64::TrapFrame& tf)
        {
            const auto ustackFlags = vm::Page_P | vm::Page_RW | vm::Page_US;
            auto& mapping = vm::Map(vs, vm::userland::stackBase, ustackFlags, vm::userland::stackSize);
            const auto ustackVA = vm::userland::stackBase + vm::userland::stackSize - vm::PageSize;
            const auto pagePhysicalAddr = page->GetPhysicalAddress();
            mapping.pages.push_back(vm::MappedPage{ ustackVA, std::move(page) });

            vm::MapMemory(vs,
                ustackVA, vm::PageSize,
                pagePhysicalAddr, ustackFlags);

            // Align the stack pointer in line with the AMD64 ELF ABI, 3.2.2
            tf.rsp = ustackVA - 8;
            assert(((tf.rsp - 8) & 0xf) == 0);
            tf.rdi = ustackVA;
        }
    } // namespace

    result::MaybeInt Exec(amd64::TrapFrame& tf)
    {
        const auto path = syscall::GetArgument<1, const char*>(tf);
        const auto argv = syscall::GetArgument<2, const char**>(tf);
        const auto envp = syscall::GetArgument<3, const char**>(tf);
        auto inode = fs::namei(path.get(), fs::Follow::Yes, {});
        if (!inode) return result::Error(inode.error());

        Elf64_Ehdr ehdr;
        if (fs::Read(**inode, reinterpret_cast<void*>(&ehdr), 0, sizeof(ehdr)) != sizeof(ehdr)) {
            return result::Error(error::Code::NotAnExecutable);
        }
        if (!VerifyHeader(ehdr)) {
            return result::Error(error::Code::NotAnExecutable);
        }

        // We must prepare the new userland stack with argc/argv/envp before
        // freeing mappings, as we need to read the current memory space
        auto& vs = process::GetCurrent().vmspace;
        auto ustack = PrepareNewUserlandStack(argv.get(), envp.get());
        vm::FreeMappings(vs);

        const auto phLoaded = LoadProgramHeaders(vs, std::move(*inode), ehdr);
        if (!phLoaded) {
            // TODO need to kill the process here
            return result::Error(error::Code::MemoryFault);
        }

        MapUserlandStack(vs, std::move(ustack), tf);
        amd64::FlushTLB();

        tf.rip = ehdr.e_entry;

        auto& current = process::GetCurrent();
        if (current.ptrace.traced) {
            current.ptrace.signal = SIGTRAP;
            current.state = process::State::Stopped;
            signal::Send(*current.parent, SIGCHLD);
            process::Yield();
        }
        return 0;
    }

    const char* ExtractArgv0(vm::VMSpace& vs, size_t max_length)
    {
        // First, locate the mapping where the userland stack is located
        auto& mappings = vs.mappings;
        auto mapping_it = std::find_if(mappings.begin(), mappings.end(), [](const auto& m) {
            return m.va_start == vm::userland::stackBase;
        });
        if (mapping_it == mappings.end()) return nullptr;

        // The first page of this mapping is contains the stack as mapped by
        // MapUserlandStack()
        assert(!mapping_it->pages.empty());
        auto& stack_vmpage = mapping_it->pages.front();

        // PrepareNewUserlandStack() will first write argc (uint64_t), followed by
        // argv. We want the contents of argv[0]
        const auto m = reinterpret_cast<const char*>(stack_vmpage.page->GetData());
        const uint64_t argv0 = *reinterpret_cast<const uint64_t*>(&m[sizeof(uint64_t)]);
        if (argv0 >= stack_vmpage.va && argv0 <= stack_vmpage.va + vm::PageSize) {
            const char* s = &m[argv0 - stack_vmpage.va];
            // Only return s if it contains a terminator within max_length bytes
            for (size_t n = 0; n < max_length; ++n) {
                if (s[n] == '\0') return s;
            }
        }
        return nullptr;
    }

}
