#include "lib.h"
#include "console.h"
#include "types.h"

namespace
{
    const char hextab[] = "0123456789abcdef";

    template<typename Emitter>
    void put8(const uint8_t v, Emitter emit)
    {
        emit(hextab[v >> 4]);
        emit(hextab[v & 0xff]);
    }

    template<typename Emitter>
    void putint(const int base, const uintmax_t n, Emitter emit)
    {
        // Determine the number of digits we need to print and the maximum divisor
        uintmax_t divisor = 1;
        unsigned int p = 0;
        for (uintmax_t i = n; i >= base; i /= base, p++, divisor *= base)
            /* nothing */;

        // Print from most-to-least significant digit
        for (unsigned int i = 0; i <= p; i++, divisor /= base)
            emit(hextab[(n / divisor) % base]);
    }

    template<typename Emitter>
    int format(const char* fmt, Emitter emit, va_list va)
    {
        int isLong = 0;
        auto fetchValue = [&]() -> uintmax_t {
            int wasLong = isLong;
            isLong = 0;
            switch (wasLong) {
                case 0:
                    return va_arg(va, unsigned int);
                case 1:
                    return va_arg(va, unsigned long);
                case 2:
                    return va_arg(va, unsigned long long);
            }
            return -1;
        };

        for (/* nothing */; *fmt != '\0'; ++fmt) {
            if (*fmt != '%') {
                emit(*fmt);
                continue;
            }

            fmt++;
            while (*fmt == 'l') {
                ++isLong, ++fmt;
            }

            switch (*fmt) {
                case 'c': {
                    char ch = va_arg(va, int);
                    emit(ch);
                    break;
                }
                case 'p': {
                    const uint64_t v = va_arg(va, unsigned long);
                    putint(16, v, emit);
                    isLong = 0;
                    break;
                }
                case 'x': {
                    const uintmax_t v = fetchValue();
                    putint(16, v, emit);
                    break;
                }
                case 's': {
                    const char* s = va_arg(va, const char*);
                    while (*s != '\0')
                        emit(*s++);
                    break;
                }
                case 'd': {
                    const uintmax_t v = fetchValue();
                    putint(10, v, emit);
                    break;
                }
                default:
                    emit('%');
                    emit(*fmt);
                    break;
            }
        }

        return 0;
    }

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

int printf(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    format(fmt, [](int ch) { console::put_char(ch); }, va);
    va_end(va);
    return 0;
}

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

void* memcpy(void* dst, const void* src, size_t len)
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

void panic(const char* s)
{
    __asm __volatile("cli");
    printf("panic: %s\n", s);
    while (1)
        ;
}
