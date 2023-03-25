#include "lib.h"
#include <cstdint>
extern "C" {
#include <efi.h>
#include <efilib.h>
}

namespace
{
    // Helper for memset
    template<typename T>
    size_t Fill(void*& d, size_t sz, T v)
    {
        auto ptr = reinterpret_cast<T*>(d);
        for (uint64_t n = 0; n < sz / sizeof(T); ++n, ++ptr)
            *ptr = v;
        d = reinterpret_cast<void*>(ptr);
        return (sz / sizeof(T)) * sizeof(T);
    }

    template<typename T>
    size_t Copy(void*& d, const void*& s, size_t sz)
    {
        auto dst = reinterpret_cast<T*>(d);
        auto src = reinterpret_cast<const T*>(s);
        for (uint64_t n = 0; n < sz / sizeof(T); ++n)
            *dst++ = *src++;
        d = reinterpret_cast<void*>(dst);
        s = reinterpret_cast<const void*>(src);
        return (sz / sizeof(T)) * sizeof(T);
    }
} // namespace

void* memset(void* p, int c, size_t len)
{
    // Optimised by aligning to a 32-bit address and doing 32-bit operations
    // while possible
    auto dest = p;

    if (len >= 4 && (reinterpret_cast<uint64_t>(dest) & 3))
        len -= Fill(dest, len & 3, static_cast<uint8_t>(c));
    const uint32_t c32 = static_cast<uint32_t>(c) << 24 | static_cast<uint32_t>(c) << 16 |
                         static_cast<uint32_t>(c) << 8 | static_cast<uint32_t>(c);
    len -= Fill(dest, len, c32);
    Fill(dest, len, static_cast<uint8_t>(c));

    return p;
}

extern "C" void* memcpy(void* dst, const void* src, size_t len)
{
    auto ret = dst;

    // Optimised by aligning to a 32-bit address and doing 32-bit operations
    // while possible
    if (len >= 4 && (reinterpret_cast<uint64_t>(dst) & 3))
        len -= Copy<uint8_t>(dst, src, len & 3);
    len -= Copy<uint32_t>(dst, src, len);
    Copy<uint8_t>(dst, src, len);
    return ret;
}

size_t strlen(const char* src) { return strchr(src, '\0') - src; }

char* strchr(const char* s, int ch)
{
    while (true) {
        if (*s == ch)
            return const_cast<char*>(s);
        if (*s == '\0')
            return nullptr;
        ++s;
    }
    // NOTREACHED
}

int strcmp(const char* a, const char* b)
{
    while (*a != '\0' && *a == *b)
        ++a, ++b;
    return *a - *b;
}

int memcmp(const char* a, const char* b, size_t len)
{
    for (size_t n = 0; n < len; ++n)
        if (a[n] != b[n])
            return a[n] - b[n];
    return 0;
}

void panic(const char* fmt, ...)
{
    Print(reinterpret_cast<const CHAR16*>(L"panic: %a\n"), fmt);
    while(1);
}

void assert_failure(const char* file, int line)
{
    Print(reinterpret_cast<const CHAR16*>(L"assertion failure in %a:%d\n"), file, line);
    while(1);
}
