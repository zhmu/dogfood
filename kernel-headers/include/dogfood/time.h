/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "types.h"

struct timeval {
    __time_t tv_sec;
    __suseconds_t tv_usec;
};
