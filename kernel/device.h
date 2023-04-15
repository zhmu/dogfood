#include "types.h"
#include <expected>
#include "error.h"

namespace device
{
    struct CharacterDevice
    {
        virtual std::expected<int, error::Code> Write(const void* buf, int len) = 0;
        virtual std::expected<int, error::Code> Read(void* buf, int len) = 0;
        virtual bool CanRead() = 0;
        virtual bool CanWrite() = 0;
    };

    CharacterDevice* LookupCharacterDevice(dev_t dev);
    CharacterDevice* LookupConsole();
}
