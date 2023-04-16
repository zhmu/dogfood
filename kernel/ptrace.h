#pragma once

#include "result.h"

namespace amd64 { struct TrapFrame; }

namespace ptrace {
    struct State {
        bool traced = false;
        bool traceSyscall = false;
        int signal{};
    };

    result::MaybeInt PTrace(amd64::TrapFrame&);
}
