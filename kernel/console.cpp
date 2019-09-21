#include "console.h"
#include "amd64.h"

using namespace amd64::io;

namespace console
{
    inline constexpr int port = 0x3f8; // COM1

    namespace registers
    {
        inline constexpr int DATA = 0; /* Data register (R/W) */
        inline constexpr int IER = 1;  /* Interrupt Enable Register */
        inline constexpr int FIFO = 2; /* Interrupt Identification and FIFO Registers */
        inline constexpr int LCR = 3;  /* Line Control Register */
        inline constexpr int MCR = 4;  /* Modem Control Register */
        inline constexpr int LSR = 5;  /* Line Status Register */
        inline constexpr int MSR = 6;  /* Modem Status Register */
        inline constexpr int SR = 7;   /* Scratch Register */
    }                                  // namespace registers

    void initialize()
    {
        outb(port + registers::IER, 0);     /* Disables interrupts */
        outb(port + registers::LCR, 0x80);  /* Enable DLAB */
        outb(port + registers::DATA, 1);    /* Divisor low byte (115200 baud) */
        outb(port + registers::IER, 0);     /* Divisor hi byte */
        outb(port + registers::LCR, 3);     /* 8N1 */
        outb(port + registers::FIFO, 0xc7); /* Enable/clear FIFO (14 bytes) */
    }

    void put_char(int ch)
    {
        while ((inb(port + registers::LSR) & 0x20) == 0)
            /* wait for the transfer buffer to become empty */;
        outb(port + registers::DATA, ch);
    }

    int get_char()
    {
        if ((inb(port + registers::LSR) & 1) == 0)
            return 0;
        return inb(port + registers::DATA);
    }

} // namespace console
