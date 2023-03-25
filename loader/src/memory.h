/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstdint>
#include <span>
#include <utility>

namespace memory {

    enum class MemoryType {
        Invalid,
        Reserved,
        ACPI,
        Usuable,
        EfiRuntimeCode,
        EfiRuntimeData,
    };

    struct Entry {
        MemoryType type;
        uint64_t phys_start;
        uint64_t phys_end;
    };

    std::pair<std::span<Entry>, unsigned int> ConstructMemoryMap();
}
