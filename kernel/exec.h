#pragma once

#include "types.h"
#include <expected>
#include "error.h"

namespace amd64
{
    struct TrapFrame;
}

namespace vm { struct VMSpace; }

namespace exec
{
    std::expected<int, error::Code> Exec(amd64::TrapFrame& tf);
    const char* ExtractArgv0(vm::VMSpace& vs, size_t max_length);
}
