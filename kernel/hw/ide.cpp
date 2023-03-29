#include "ide.h"
#include "../x86_64/amd64.h"
#include "../bio.h"
#include "../process.h"
#include "../lib.h"
#include "pic.h"

using namespace amd64::io;

namespace ide
{
    namespace
    {
        namespace io
        {
            constexpr unsigned int Port = 0x1f0;
            constexpr unsigned int AltPort = 0x3f6;
        } // namespace io
        namespace port
        {
            constexpr unsigned int Data = 0;
            constexpr unsigned int DeviceControl = 2;
            constexpr unsigned int SectorCount = 2;
            constexpr unsigned int SectorNumber = 3;
            constexpr unsigned int CylinderLo = 4;
            constexpr unsigned int CylinderHi = 5;
            constexpr unsigned int DeviceHead = 6;
            constexpr unsigned int Status = 7;
            constexpr unsigned int Command = 7;
        } // namespace port
        namespace status
        {
            constexpr unsigned int Error = (1 << 0);
            constexpr unsigned int DataRequest = (1 << 3);
            constexpr unsigned int DeviceReady = (1 << 6);
            constexpr unsigned int Busy = (1 << 7);
        } // namespace status
        namespace command
        {
            constexpr unsigned int ReadSectors = 0x20;  // 28 bit PIO
            constexpr unsigned int WriteSectors = 0x30; // 28 bit PIO
        }                                               // namespace command
        namespace control {
            constexpr unsigned int SRST = (1 << 2);
        }

        bool IsWrite(const bio::Buffer& buffer) { return (buffer.flags & bio::flag::Dirty) != 0; }
        unsigned int ReadStatus() { return inb(io::AltPort); }

        bio::Buffer* queue = nullptr;

        bool IsQueued(bio::Buffer& buffer)
        {
            auto q = queue;
            while (q != nullptr) {
                if (q == &buffer) return true;
                q = q->qnext;
            }
            return false;
        }
    } // namespace

    void Initialize()
    {
        outb(io::Port + port::DeviceControl, 0);
        pic::Enable(pic::irq::IDE);
        // Reset all devices on the bus
        outb(io::AltPort, control::SRST);
        outb(io::AltPort, 0);

        bio::RegisterDevice(0, 0);

    }

    void ExecuteIO(bio::Buffer& buffer)
    {
        const bool isWrite = (buffer.flags & bio::flag::Dirty) != 0;
        const auto command = isWrite ? command::WriteSectors : command::ReadSectors;

        const auto lba = buffer.ioBlockNumber;
        const int unit = 0;
        outb(
            io::Port + port::DeviceHead,
            0xe0 | (unit ? 0x10 : 0) | ((lba >> 24) & 0xf));
        outb(io::Port + port::SectorCount, 1);
        outb(io::Port + port::SectorNumber, lba & 0xff);
        outb(io::Port + port::CylinderLo, (lba >> 8) & 0xff);
        outb(io::Port + port::CylinderHi, (lba >> 16) & 0xff);
        outb(io::Port + port::Command, command);
        if (isWrite) {
            while (ReadStatus() & status::Busy)
                /* nothing */;
            while (1) {
                uint8_t status = ReadStatus();
                if (status & status::Error) {
                    /* Got an error - this means the request cannot be completed */
                    Print("IDE: error on write\n");
                    return;
                }
                if (status & status::DataRequest)
                    break;
            }

            /* XXX We really need outsw() or similar */
            for (int n = 0; n < bio::BlockSize; n += 2) {
                uint16_t v = buffer.data[n] | static_cast<uint16_t>(buffer.data[n + 1]) << 8;
                outw(io::Port + port::Data, v);
            }
        }
    }

    void OnIRQ()
    {
        int stat = inb(io::Port + port::Status);
        if (stat & status::Error)
            panic("ide::OnIRQ() with error status");

        if (!queue) return;
        bio::Buffer* buffer = queue;
        queue = queue->qnext;

        if (!IsWrite(*buffer)) {
            for (int n = 0; n < bio::BlockSize; n += 2) {
                const uint16_t v = inw(io::Port + port::Data);
                buffer->data[n] = v & 0xff;
                buffer->data[n + 1] = v >> 8;
            }
            buffer->flags |= bio::flag::Valid;
        }
        buffer->flags &= ~bio::flag::Dirty;
        process::Wakeup(buffer);

        if (queue != nullptr)
            ExecuteIO(*queue);
    }

    void PerformIO(bio::Buffer& buffer)
    {
        auto state = interrupts::SaveAndDisable();

        if (IsQueued(buffer)) {
            // Already queued; just wait until the request is handled
            process::Sleep(&buffer);
            interrupts::Restore(state);
            return;
        }

        // Append buffer to queue
        {
            buffer.qnext = nullptr;
            if (queue != nullptr) {
                auto q = queue;
                while (q->qnext != nullptr)
                    q = q->qnext;
                q->qnext = &buffer;
            } else
                queue = &buffer;
        }

        if (queue == &buffer)
            ExecuteIO(buffer);

        while ((buffer.flags & (bio::flag::Valid | bio::flag::Dirty)) != bio::flag::Valid) {
            process::Sleep(&buffer);
        }
        interrupts::Restore(state);
    }

} // namespace ide
