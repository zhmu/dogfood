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
#include <dogfood/loader.h>

namespace memory {
    std::pair<std::span<loader::memory::Entry>, unsigned int> ConstructMemoryMap();
}
