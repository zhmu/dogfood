#pragma once

#include "types.h"
#include <array>
#include <deque>
#include <expected>
#include "error.h"

namespace amd64 { struct TrapFrame; }

namespace pipe
{
    constexpr static auto inline PipeBufferSize = 1024;

    struct Pipe
    {
        int p_num_readers{};
        int p_num_writers{};
        std::deque<uint8_t> p_deque{};
        size_t p_buffer_readpos{};
        size_t p_buffer_writepos{};

        std::expected<int, error::Code> Read(void* buf, int len, const bool nonblock);
        std::expected<int, error::Code> Write(const void* buf, int len);

        bool CanRead();
        bool CanWrite();

        size_t DetermineMaximumWriteSize() const;
    };

    std::expected<int, error::Code> pipe(amd64::TrapFrame& tf);
}
