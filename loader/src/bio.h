/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace bio {
    using Device = int;
    using BlockNumber = uint64_t;
    constexpr size_t inline BlockSize = 512;

    struct Buffer {
        Device device;
        BlockNumber blockNr;
        uint8_t data[BlockSize];
    };

    Device GetNumberOfDevices();

    void Initialize();
    Buffer& bread(Device, BlockNumber);
    void bwrite(Buffer&);
    void brelse(Buffer&);
}
