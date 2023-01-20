#pragma once

#include "types.h"
#include <array>

namespace amd64 { struct TrapFrame; }

namespace pipe
{
    constexpr static auto inline PipeBufferSize = 1024;

    struct Pipe
    {
        int p_num_readers{};
        int p_num_writers{};
        std::array<uint8_t, PipeBufferSize> p_buffer{};
        size_t p_buffer_readpos{};
        size_t p_buffer_writepos{};

        int Read(void* buf, int len, const bool nonblock);
        int Write(const void* buf, int len);

        bool CanRead();
        bool CanWrite();
    };

    int pipe(amd64::TrapFrame& context);
}
