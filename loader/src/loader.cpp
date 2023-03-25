extern "C" {
#include <efi.h>
#include <efilib.h>
}
#include <cstddef>
#include <span>
#include "bio.h"
#include "kernel.h"
#include "memory.h"
#include "heap.h"
#include "fs.h"

namespace
{
    static constexpr auto inline NumberOfHeapPages = 16; // 64KB
    static constexpr auto inline KernelFile = "/kernel.elf";
    EFI_PHYSICAL_ADDRESS heapBase;
}

extern "C" EFI_STATUS
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS status;

    InitializeLib(image, systab);
    Print(reinterpret_cast<const CHAR16*>(L">> Dogfood UEFI loader\n"));

    status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, NumberOfHeapPages, &heapBase);
    if (status != EFI_SUCCESS) {
        Print(reinterpret_cast<const CHAR16*>(L"cannot allocate heap!\n"));
        return EFI_ABORTED;
    }
    heap::InitializeHeap(reinterpret_cast<void*>(heapBase), NumberOfHeapPages * EFI_PAGE_SIZE);
    bio::Initialize();

    EFI_PHYSICAL_ADDRESS elfStorage;
    status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &elfStorage);
    if (status != EFI_SUCCESS) {
        Print(reinterpret_cast<const CHAR16*>(L"cannot allocate elf storage!\n"));
        return EFI_ABORTED;
    }

    for(bio::Device dev{}; dev < bio::GetNumberOfDevices(); ++dev) {
        if (!fs::Mount(dev)) {
            Print(reinterpret_cast<const CHAR16*>(L"cannot mount block device %d\n"), dev);
            continue;
        }

        auto inode = fs::namei(KernelFile, true, nullptr);
        if (inode == nullptr) {
            Print(reinterpret_cast<const CHAR16*>(L"device %d: cannot find '%a'\n"), dev, KernelFile);
            continue;
        }

        // Read the first 4KB; this ought to contain all ELF headers and such
        auto elf = reinterpret_cast<uint8_t*>(elfStorage);
        if (fs::Read(*inode, elf, 0, 4096) != 4096) {
            Print(reinterpret_cast<const CHAR16*>(L"device %d: cannot read '%a'\n"), dev, KernelFile);
            continue;
        }

        Print(reinterpret_cast<const CHAR16*>(L"Trying to load kernel from device %d...\n"), dev);
        if (kernel::Load(*inode, { &elf[0], &elf[4096] })) {
            kernel::Execute();
        }
    }

    Print(reinterpret_cast<const CHAR16*>(L"No suitable kernel found, aborting\n"));
    return EFI_ABORTED;
}
