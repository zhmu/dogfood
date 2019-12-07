#pragma once

#include <functional>

namespace bio { struct Buffer; }

namespace test_stubs {
    void ResetFunctions();
    void SetPerformIOFunction(std::function<void(bio::Buffer&)>&& fn);
    void SetPutCharFunction(std::function<void(int)>&& fn);
}
