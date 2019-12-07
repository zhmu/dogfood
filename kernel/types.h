#pragma once

using __int8_t = signed char;
using __uint8_t = unsigned char;

using __int16_t = signed short;
using __uint16_t = unsigned short;

using __int32_t = signed int;
using __uint32_t = unsigned int;

using __int64_t = signed long;
using __uint64_t = unsigned long;

using intmax_t = __int64_t;
using uintmax_t = __uint64_t;

using size_t = __uint64_t;
using off_t = __int64_t;

using int8_t = __int8_t;
using uint8_t = __uint8_t;
using int16_t = __int16_t;
using uint16_t = __uint16_t;
using int32_t = __int32_t;
using uint32_t = __uint32_t;
using int64_t = __int64_t;
using uint64_t = __uint64_t;

typedef __builtin_va_list va_list;
#define va_arg __builtin_va_arg
#define va_start __builtin_va_start
#define va_end __builtin_va_end

#include "dogfood/types.h"

#ifndef BUILDING_TESTS
using dev_t = __dev_t;
using uid_t = __uid_t;
using ino_t = __ino_t;
using mode_t = __mode_t;
using nlink_t = __nlink_t;
using blksize_t = __blksize_t;
using blkcnt_t = __blkcnt_t;
using time_t = __time_t;
#endif
