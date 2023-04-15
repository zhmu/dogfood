#include "device.h"

#include <dogfood/device.h>
#include "hw/console.h"

namespace device
{
    namespace
    {
        struct NullDevice : CharacterDevice {
            std::expected<int, error::Code> Write(const void* buf, int len) override { return len; }
            std::expected<int, error::Code> Read(void* buf, int len) override { return 0; }
            bool CanRead() override { return false; }
            bool CanWrite() override { return true; }
        } nullDevice;

        struct ConsoleDevice : CharacterDevice {
            std::expected<int, error::Code> Write(const void* buf, int len) override { return console::Write(buf, len); }
            std::expected<int, error::Code> Read(void* buf, int len) override { return console::Read(buf, len); }
            bool CanRead() override { return console::CanRead(); }
            bool CanWrite() override { return console::CanWrite(); }
        } consoleDevice;
    }

    CharacterDevice* LookupConsole()
    {
        return &consoleDevice;
    }

    CharacterDevice* LookupCharacterDevice(dev_t dev)
    {
        const auto major = (dev >> DOGFOOD_DEV_MAJOR_SHIFT) & DOGFOOD_DEV_MAJOR_MASK;
        const auto minor = (dev >> DOGFOOD_DEV_MINOR_SHIFT) & DOGFOOD_DEV_MINOR_MASK;
        switch(major) {
            case 1: return &nullDevice; // null
            case 2: return &consoleDevice; // console
        }
        return nullptr;
    }
}
