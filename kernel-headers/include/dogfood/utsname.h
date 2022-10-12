/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "types.h"

#define _UTS_MAX_SYSNAME_LEN    16
#define _UTS_MAX_NODENAME_LEN   16
#define _UTS_MAX_RELEASE_LEN    16
#define _UTS_MAX_VERSION_LEN    16
#define _UTS_MAX_MACHINE_LEN    16

struct utsname {
    char sysname[_UTS_MAX_SYSNAME_LEN];
    char nodename[_UTS_MAX_NODENAME_LEN];
    char release[_UTS_MAX_RELEASE_LEN];
    char version[_UTS_MAX_VERSION_LEN];
    char machine[_UTS_MAX_MACHINE_LEN];
};
