/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <dogfood/errno.h>

namespace error {
    enum class Code {
        AlreadyExists = EEXIST,
        MemoryFault = EFAULT,
        InvalidArgument = EINVAL,
        IOError = EIO,
        LoopDetected = ELOOP,
        Access = EACCES,
        NameTooLong = ENAMETOOLONG,
        NoFile = ENFILE,
        NoDevice = ENODEV,
        NoEntry = ENOENT,
        OutOfSpace = ENOSPC,
        NotADirectory = ENOTDIR,
        NotEmpty = ENOTEMPTY,
        PermissionDenied = EPERM,
        NotFound = ESRCH,
    };
}
