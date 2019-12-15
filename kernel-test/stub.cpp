#include "bio.h"
#include "stub.h"

namespace test_stubs {
    namespace {
        std::function<void(bio::Buffer&)> ioFunction;
        std::function<void(int)> putCharFunction;
    }

    void SetPerformIOFunction(std::function<void(bio::Buffer&)>&& fn)
    {
        ioFunction = std::move(fn);
    }

    void SetPutCharFunction(std::function<void(int)>&& fn) {
        putCharFunction = fn;
    }

    void ResetFunctions()
    {
        ioFunction = nullptr;
        putCharFunction = nullptr;
    }
}

namespace ide
{
    void PerformIO(bio::Buffer& buffer)
    {
        test_stubs::ioFunction(buffer);
    }

    void OnIRQ()
    {
    }
}

namespace console
{
    void put_char(int ch)
    {
        test_stubs::putCharFunction(ch);
    }

    int Read(void* buf, int len)
    {
        return 0;
    }

    int Write(const void* buf, int len)
    {
        return 0;
    }

    void OnIRQ()
    {
    }
}

namespace pic
{
    void Acknowledge()
    {
    }
}
