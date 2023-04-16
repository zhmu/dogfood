/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <expected>
#include "error.h"

namespace result {
    template<typename T> using Maybe = std::expected<T, error::Code>;

    using Error = std::unexpected<error::Code>;

    using MaybeInt = Maybe<int>;
}
