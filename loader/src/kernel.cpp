extern "C" {
#include <efi.h>
#include <efilib.h>
extern EFI_HANDLE LibImageHandle;
}
#include "elf.h"
#include "fs.h"
#include "lib.h"
#include "memory.h"
#include <span>
#include "multiboot.h"

namespace kernel
{

namespace
{
    Elf64_Ehdr* kernelEhdr;
    std::array<uint64_t, 8> phdrPhysAddress;

    bool VerifyHeader(const Elf64_Ehdr& ehdr, const size_t header_length)
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

        // See if all the program headers are in range
        for (int ph = 0; ph < ehdr.e_phnum; ++ph) {
            const auto offset = ehdr.e_phoff + ph * sizeof(Elf64_Phdr);
            if (offset + sizeof(Elf64_Phdr) < offset || offset + sizeof(Elf64_Phdr) > header_length)
                return false;
        }

        // Ensure we have enough spots
        assert(ehdr.e_phnum < phdrPhysAddress.size());
        return true;
    }

    bool LoadImage(const Elf64_Ehdr& ehdr, std::span<uint8_t> headers, fs::Inode& inode)
    {
        for (int ph = 0; ph < ehdr.e_phnum; ++ph) {
            auto& phdr = *reinterpret_cast<Elf64_Phdr*>(&headers[ehdr.e_phoff + ph * sizeof(Elf64_Phdr)]);
            if (phdr.p_type != PT_LOAD) continue;

            const auto numberOfPages = (phdr.p_memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
            EFI_PHYSICAL_ADDRESS phBase;
            const auto status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, numberOfPages, &phBase);
            if (status != EFI_SUCCESS) return false;

            Print(reinterpret_cast<const CHAR16*>(L"Reading %d bytes from offset %lx to address %lx\n"), phdr.p_filesz, phdr.p_offset, phBase);
            if (fs::Read(inode, reinterpret_cast<void*>(phBase), phdr.p_offset, phdr.p_filesz) != phdr.p_filesz)
                return false;

            phdrPhysAddress[ph] = phBase;
        }
        return true;
    }
}

bool Load(fs::Inode& inode, std::span<uint8_t> headers)
{
    if (headers.size() < sizeof(Elf64_Ehdr))
        return false;

    auto& ehdr = *reinterpret_cast<Elf64_Ehdr*>(headers.data());
    if (!VerifyHeader(ehdr, headers.size())) return false;
    if (!LoadImage(ehdr, headers, inode)) return false;

    kernelEhdr = &ehdr;
    return true;
}

void Execute()
{
    auto headerBase = reinterpret_cast<uint8_t*>(kernelEhdr);
    assert(headerBase != nullptr);

    // Grab memory now that we can still print information if it fails
    constexpr auto mb_mmap_max_entries = EFI_PAGE_SIZE / sizeof(MULTIBOOT_MMAP);
    auto mmap = new MULTIBOOT_MMAP[mb_mmap_max_entries];

    // Create multiboot structure
    auto mb = new MULTIBOOT{};
    assert(reinterpret_cast<uint64_t>(mmap) <= 0xffff'ffff);
    mb->mb_mmap_addr = reinterpret_cast<uint64_t>(mmap) & 0xffff'ffff;

    // Grab 3 pages for bootstrap pagemap; these must be page-aligned
    EFI_PHYSICAL_ADDRESS pd_phys;
    const auto status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 3, &pd_phys);
    if (status != EFI_SUCCESS) panic("can't allocate bootstrap pages");
    auto pd = reinterpret_cast<uint64_t*>(pd_phys);
    uint64_t pa = 0;
    for(int n = 0; n < 512; ++n) {
        pd[n + 0] = (pd_phys + 4096) | 0x3; // pdpe base | RW | P
        pd[n + 512] = (pd_phys + 8192) | 0x3; // pde base | RW | P
        pd[n + 1024] = pa | 0x83; // pdpe entry (phys addr | PS | RW | P)
        pa += 0x200000;
    }

    auto [ memory_map, map_key ] = memory::ConstructMemoryMap();
    const auto result = BS->ExitBootServices(LibImageHandle, map_key);
    if (result != EFI_SUCCESS) {
        panic("ExitBootServices() failed");
    }

    // Construct multiboot memory map
    size_t n = 0;
    for(const auto& m: memory_map) {
        auto& mm = mmap[n++];
        mm.mm_entry_len = sizeof(MULTIBOOT_MMAP) - sizeof(uint32_t);
        mm.mm_base_hi = m.phys_start >> 32;
        mm.mm_base_lo = m.phys_start & 0xffff'ffff;
        const auto length = m.phys_end - m.phys_start;
        mm.mm_len_lo = length & 0xffff'ffff;
        mm.mm_len_hi = length >> 32;
        mm.mm_type = 0;
        switch(m.type) {
            case memory::MemoryType::Usuable:
            case memory::MemoryType::EfiRuntimeCode:
            case memory::MemoryType::EfiRuntimeData:
                mm.mm_type = MULTIBOOT_MMAP_AVAIL;
                break;
            default:
                mm.mm_type = 0;
                break;

        }
    }
    mb->mb_mmap_length = mb->mb_mmap_addr + n * sizeof(MULTIBOOT_MMAP);

    // We are in full control of memory now! Just memcpy stuff in the correct place
    // TODO: We could just leave it there and tell the kernel, but that is for later
    for (int ph = 0; ph < kernelEhdr->e_phnum; ++ph) {
        auto& phdr = *reinterpret_cast<Elf64_Phdr*>(&headerBase[kernelEhdr->e_phoff + ph * sizeof(Elf64_Phdr)]);
        if (phdr.p_type != PT_LOAD) continue;

        auto dst = reinterpret_cast<char*>(phdr.p_paddr);
        memcpy(&dst[0], reinterpret_cast<char*>(phdrPhysAddress[ph]), phdr.p_filesz);
        memset(&dst[phdr.p_filesz], 0, phdr.p_memsz - phdr.p_filesz);
    }

    const auto entry = kernelEhdr->e_entry;
    __asm __volatile(
        "cli\n"
        "movq %%rax, %%cr3\n"
        "jmp *%%rbx\n"
    : : "a" (pd_phys), "b" (entry), "D" (mb));

    for(;;);
}

}
