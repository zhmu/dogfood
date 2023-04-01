/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

namespace loader {
    namespace memory {
        enum class Type {
            Invalid,
            Reserved,
            ACPI,
            Usable,
            EfiRuntimeCode,
            EfiRuntimeData,
        };

        struct Entry {
            Type type;
            uint64_t phys_addr;
            uint64_t length_in_bytes;
        };
    }

    struct Channel
    {
        size_t num_memory_map_entries;
        const memory::Entry* memory_map;
    };
}
