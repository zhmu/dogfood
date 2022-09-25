#include "lib.h"
#include "types.h"
#include "x86_64/amd64.h"

#include "hw/console.h"

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
    amd64::interrupts::Disable();
    Print("panic: ", s, "\n");
    while (1)
        ;
}

size_t strlen(const char* src) { return strchr(src, '\0') - src; }

size_t strlcpy(char* dst, const char* src, size_t len)
{
    assert(len > 0);

    auto src_len = strlen(src);
    if (src_len > len)
        src_len = len;
    memcpy(dst, src, len);
    dst[len - 1] = '\0';
    return src_len;
}

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

extern "C" void* memmove(void* dst, const void* src, size_t len)
{
    auto dst_c = static_cast<char*>(dst);
    auto src_c = static_cast<const char*>(src);

    if (dst_c <= src_c) {
        while(len--) {
            *dst_c++ = *src_c++;
        }
    } else {
        src_c += len;
        dst_c += len;
        while(len--) {
            *--dst_c = *--src_c;
        }
    }
    return dst;
}

namespace print::detail {
    void Print(const char* s)
    {
        for(; *s != '\0'; ++s) console::put_char(*s);
    }

    void Print(uintmax_t n)
    {
        putint(10, n, [](const auto ch) { console::put_char(ch); });
    }

    void Print(Hex n)
    {
        putint(16, n.v, [](const auto ch) { console::put_char(ch); });
    }
}

namespace std {
    void __throw_bad_array_new_length() { panic("__throw_bad_array_new_length"); }
    void __throw_bad_alloc() { panic("__throw_bad_alloc"); }
    void __throw_length_error(const char* s) { panic("__throw_length_error"); }
}

extern "C" int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle)
{
}
