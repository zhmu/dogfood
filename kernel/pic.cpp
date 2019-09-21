#include "pic.h"
#include "amd64.h"

using namespace amd64::io;

namespace pic
{
    namespace
    {
        namespace pic1
        {
            inline constexpr unsigned int BASE = 0x20;
            inline constexpr unsigned int CMD = BASE;
            inline constexpr unsigned int DATA = BASE + 1;
        } // namespace pic1
        namespace pic2
        {
            inline constexpr unsigned int BASE = 0xa0;
            inline constexpr unsigned int CMD = BASE;
            inline constexpr unsigned int DATA = BASE + 1;
        } // namespace pic2

        namespace icw1
        {
            inline constexpr unsigned int ICW4 = 0x01;
            inline constexpr unsigned int INIT = 0x10;
        } // namespace icw1
        namespace icw4
        {
            inline constexpr unsigned int MODE_8086 = 0x01;
        } // namespace icw4

        uint16_t picMask = 0xffff & (~(1 << irq::Slave)); // enable PIC2 slave

        void ApplyMask()
        {
            outb(pic1::DATA, picMask & 0xff);
            outb(pic2::DATA, picMask >> 8);
        }
    } // namespace

    void Initialize()
    {
        constexpr auto io_wait = []() {
            for (int i = 0; i < 10; i++)
                /* nothing */;
        };

        // Start initialization: the PIC will wait for 3 command bytes
        outb(pic1::CMD, icw1::INIT | icw1::ICW4);
        io_wait();
        outb(pic2::CMD, icw1::INIT | icw1::ICW4);
        io_wait();
        // Data byte 1 is the interrupt vector offset - program for interrupt 0x20-0x2f
        outb(pic1::DATA, 0x20);
        io_wait();
        outb(pic2::DATA, 0x28);
        io_wait();
        // Data byte 2 tells the PIC how they are wired
        outb(pic1::DATA, 0x04);
        io_wait();
        outb(pic2::DATA, 0x02);
        io_wait();
        // Data byte 3 contains environment flags
        outb(pic1::DATA, icw4::MODE_8086);
        io_wait();
        outb(pic2::DATA, icw4::MODE_8086);
        io_wait();
        ApplyMask();
    }

    void Acknowledge()
    {
        outb(pic1::CMD, 0x20);
        outb(pic2::CMD, 0x20);
    }

    void Enable(int irq)
    {
        picMask &= ~(1 << irq);
        ApplyMask();
    }
} // namespace pic
