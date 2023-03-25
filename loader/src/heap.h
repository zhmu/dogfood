/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstddef>

namespace heap {
    void InitializeHeap(void* ptr, size_t number_of_bytes);
}
