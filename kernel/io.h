#pragma one

#include "types.h"

namespace io
{
    inline void outb(uint16_t port, uint8_t data)
    {
        __asm volatile("outb %0, %w1" : : "a"(data), "d"(port));
    }

    inline void outw(uint16_t port, uint16_t data)
    {
        __asm volatile("outw %0, %w1" : : "a"(data), "d"(port));
    }

    inline void outl(uint16_t port, uint32_t data)
    {
        __asm volatile("outl %0, %w1" : : "a"(data), "d"(port));
    }

    inline uint8_t inb(uint16_t port)
    {
        uint8_t a;
        __asm volatile("inb %w1, %0" : "=a"(a) : "d"(port));
        return a;
    }

    inline uint16_t inw(uint16_t port)
    {
        uint16_t a;
        __asm volatile("inw %w1, %0" : "=a"(a) : "d"(port));
        return a;
    }

} // namespace io
