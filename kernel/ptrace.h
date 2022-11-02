#pragma once

namespace amd64 { struct TrapFrame; }

namespace ptrace {
    struct State {
        bool traced = false;
        bool traceSyscall = false;
        int signal{};
    };

    long PTrace(amd64::TrapFrame&);
}
