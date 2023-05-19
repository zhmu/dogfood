/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "types.h"

#define PTRACE_TRACEME  1
#define PTRACE_ATTACH   2
#define PTRACE_DETACH   3
#define PTRACE_SYSCALL  4
#define PTRACE_GETREGS  5
#define PTRACE_PEEK     6
#define PTRACE_CONT     7
#define PTRACE_SETOPTIONS 8
# define PTRACE_O_TRACEFORK (1 << 0)
