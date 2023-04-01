/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
extern "C" {
#include <efi.h>
#include <efilib.h>
}
#include <cstddef>
#include <cstring>
#include <algorithm>
#include "dogfood/loader.h"
#include "memory.h"
#include "lib.h"

namespace memory {

namespace {

constexpr auto inline SHOW_LOADER_MEMORY_MAP = false;

void sort_items(std::span<char> descriptor_map, const size_t descriptor_size)
{
    // Uses bubble-sort
    size_t n = descriptor_map.size() / descriptor_size;
    while(true) {
        bool swapped = false;
        for (size_t i = 1; i < n; ++i) {
            auto previous_item = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(&descriptor_map[(i - 1) * descriptor_size]);
            auto current_item = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(&descriptor_map[i * descriptor_size]);
            if (previous_item->PhysicalStart > current_item->PhysicalStart) {
                std::swap(*previous_item, *current_item);
                swapped = true;
            }
        }
        if (!swapped) break;
        --n;
    }
}

loader::memory::Type ConvertEfiMemoryType(const UINT32 v)
{
    const auto type = static_cast<EFI_MEMORY_TYPE>(v);
    switch(type) {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
            return loader::memory::Type::Usable;
        case EfiRuntimeServicesCode:
            return loader::memory::Type::EfiRuntimeCode;
        case EfiRuntimeServicesData:
            return loader::memory::Type::EfiRuntimeData;
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
        case EfiPalCode:
        case EfiReservedMemoryType:
            return loader::memory::Type::Reserved;
        case EfiACPIReclaimMemory:
        case EfiACPIMemoryNVS:
            return loader::memory::Type::ACPI;
        case EfiUnusableMemory:
        default:
            return loader::memory::Type::Invalid;
    }
}

std::span<loader::memory::Entry> merge_items(std::span<const char> descriptor_map, const size_t descriptor_size)
{
    auto result = new loader::memory::Entry[descriptor_map.size() / descriptor_size];

    size_t num_results = 0;
    for(size_t offset = 0; offset < descriptor_map.size() - descriptor_size; offset += descriptor_size) {
        auto descriptor = reinterpret_cast<const EFI_MEMORY_DESCRIPTOR*>(&descriptor_map[offset]);

        const auto type = ConvertEfiMemoryType(descriptor->Type);
        const auto start = descriptor->PhysicalStart;
        const auto end = start + descriptor->NumberOfPages * EFI_PAGE_SIZE;

        auto& current_entry = result[num_results - 1];
        if (num_results > 0 && current_entry.type == type && (current_entry.phys_addr + current_entry.length_in_bytes) == start) {
            current_entry.length_in_bytes += end - start;
        } else {
            result[num_results] = { type, start, end - start };
            ++num_results;
        }
    }

    return { &result[0], &result[num_results] };
}

std::span<loader::memory::Entry> ProcessMemoryMap(std::span<char> descriptor_map, const size_t descriptor_size)
{
    sort_items(descriptor_map, descriptor_size);
    auto items = merge_items(descriptor_map, descriptor_size);

    if constexpr (SHOW_LOADER_MEMORY_MAP) {
        Print(reinterpret_cast<const CHAR16*>(L"Loader memory map\n"));
        for(const auto& item: items) {
            Print(reinterpret_cast<const CHAR16*>(L"%lx..%lx type %d\n"), item.phys_addr, item.phys_addr + item.length_in_bytes - 1, item.type);
        }
    }
    return items;
}

}

std::pair<std::span<loader::memory::Entry>, unsigned int> ConstructMemoryMap()
{
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;
    BS->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);

    map_size += 10 * sizeof(EFI_MEMORY_DESCRIPTOR); // add some slack
    auto descriptor_map = new char[map_size];
    const auto status = BS->GetMemoryMap(&map_size, reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(descriptor_map), &map_key, &descriptor_size, &descriptor_version);
    if (status != EFI_SUCCESS) panic("cannot retrieve memory map");

    auto map = memory::ProcessMemoryMap({ &descriptor_map[0], &descriptor_map[map_size] }, descriptor_size);

    // Make sure the map_key is up to date
    BS->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);
    return { map, map_key };
}

}
