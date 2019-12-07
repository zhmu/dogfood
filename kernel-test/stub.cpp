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

    void SetPutCharFunction(std::function<void(int)> fn) {
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
}

namespace console
{
    void put_char(int ch)
    {
        test_stubs::putCharFunction(ch);
    }
}
