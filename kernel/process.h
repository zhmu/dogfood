#pragma once

#include "types.h"

namespace amd64
{
    struct Context;
    struct TrapFrame;
} // namespace amd64

namespace process
{
    enum class State { Unused, Construct, Runnable, Running, Zombie };

    struct Process {
        State state = State::Unused;
        int pid = -1;
        Process* parent = nullptr;
        //
        uint64_t pageDirectory = 0;  // physical address
        void* userStack = nullptr;   // start of user stack
        void* kernelStack = nullptr; // start of kernel stack
        struct amd64::TrapFrame* trapFrame = nullptr;
        struct amd64::Context* context = nullptr;
    };

    void Initialize();
    void Scheduler();

} // namespace process
