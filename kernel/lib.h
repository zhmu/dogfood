#pragma once

#include "types.h"

#define TS(x) _TS(x)
#define _TS(x) #x
#define _PANIC_LOCATION __FILE__ ":" TS(__LINE__)

#define assert(x) \
    ((x) ? (void)0 : panic("assertion failure: " _PANIC_LOCATION " condition: " TS(x)))

void* memset(void* p, int c, size_t n);
extern "C" {
    void* memcpy(void* dst, const void* src, size_t len);
    void* memmove(void* dst, const void* src, size_t len);
}
size_t strlcpy(char* dst, const char* src, size_t len);
char* strchr(const char* s, int ch);
int strcmp(const char* a, const char* b);
int memcmp(const char* a, const char* b, size_t len);
[[noreturn]] void panic(const char* s);
size_t strlen(const char* src);

namespace print {
    struct Hex { uintmax_t v; };

    namespace detail {
        void Print(const char*);
        void Print(uintmax_t);
        void Print(Hex);
        template<typename T> void Print(T* p) {
            Print(Hex{ reinterpret_cast<uint64_t>(p) });
        }
    }
}

template<typename... Ts> void Print(Ts... t)
{
    (print::detail::Print(t), ...);
}
