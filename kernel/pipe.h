#pragma once

#include "types.h"
#include <array>
#include <deque>
#include "result.h"

namespace amd64 { struct TrapFrame; }

namespace pipe
{
    struct Pipe
    {
        int p_num_readers{};
        int p_num_writers{};
        std::deque<uint8_t> p_deque{};
        size_t p_buffer_readpos{};
        size_t p_buffer_writepos{};

        result::MaybeInt Read(void* buf, int len, const bool nonblock);
        result::MaybeInt Write(const void* buf, int len);

        bool CanRead();
        bool CanWrite();

        size_t DetermineMaximumWriteSize() const;
    };

    result::MaybeInt pipe(amd64::TrapFrame& tf);
}
