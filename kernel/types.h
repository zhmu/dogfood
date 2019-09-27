#pragma once

using int8_t = signed char;
using uint8_t = unsigned char;

using int16_t = signed short;
using uint16_t = unsigned short;

using int32_t = signed int;
using uint32_t = unsigned int;

using int64_t = signed long;
using uint64_t = unsigned long;

using intmax_t = int64_t;
using uintmax_t = uint64_t;

using size_t = uint64_t;
using off_t = int64_t;

typedef __builtin_va_list va_list;
#define va_arg __builtin_va_arg
#define va_start __builtin_va_start
#define va_end __builtin_va_end
