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

} // namespace

int printf(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    format(fmt, [](int ch) { console::put_char(ch); }, va);
    va_end(va);
    return 0;
}
