#pragma once

#include "types.h"

namespace amd64
{
    struct TrapFrame;
}

namespace vm { struct VMSpace; }

namespace exec
{
    int Exec(amd64::TrapFrame&);
    const char* ExtractArgv0(vm::VMSpace& vs, size_t max_length);
}
