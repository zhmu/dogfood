/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstdint>
#include <span>

namespace fs { struct Inode; }

namespace kernel
{
bool Load(fs::Inode& inode, std::span<uint8_t> headers);
void Execute();
}
