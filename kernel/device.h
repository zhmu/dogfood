#include "types.h"

namespace device
{
    struct CharacterDevice
    {
        virtual int Write(const void* buf, int len) = 0;
        virtual int Read(void* buf, int len) = 0;
        virtual bool CanRead() = 0;
        virtual bool CanWrite() = 0;
    };

    CharacterDevice* LookupCharacterDevice(dev_t dev);
    CharacterDevice* LookupConsole();
}
