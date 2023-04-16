#pragma once

#include "types.h"
#include "result.h"

namespace amd64
{
    struct TrapFrame;
}

namespace vm { struct VMSpace; }

namespace exec
{
    result::MaybeInt Exec(amd64::TrapFrame& tf);
    const char* ExtractArgv0(vm::VMSpace& vs, size_t max_length);
}
