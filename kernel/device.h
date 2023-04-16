#include "types.h"
#include "result.h"

namespace device
{
    struct CharacterDevice
    {
        virtual result::MaybeInt Write(const void* buf, int len) = 0;
        virtual result::MaybeInt Read(void* buf, int len) = 0;
        virtual bool CanRead() = 0;
        virtual bool CanWrite() = 0;
    };

    CharacterDevice* LookupCharacterDevice(dev_t dev);
    CharacterDevice* LookupConsole();
}
