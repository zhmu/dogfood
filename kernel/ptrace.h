#pragma once

#include <expected>
#include "error.h"

namespace amd64 { struct TrapFrame; }

namespace ptrace {
    struct State {
        bool traced = false;
        bool traceSyscall = false;
        int signal{};
    };

    std::expected<int, error::Code> PTrace(amd64::TrapFrame&);
}
