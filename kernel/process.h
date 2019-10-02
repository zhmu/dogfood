#pragma once

#include "types.h"
#include "fs.h"

namespace amd64
{
    struct Context;
    struct Syscall;
    struct TrapFrame;
} // namespace amd64

namespace process
{
    enum class State { Unused, Construct, Runnable, Running, Zombie, Sleeping };

    struct Process {
        State state = State::Unused;
        int pid = -1;
        int ppid = -1;
        Process* parent = nullptr;
        //
        uint64_t pageDirectory = 0;  // physical address
        void* userStack = nullptr;   // start of user stack
        void* kernelStack = nullptr; // start of kernel stack
        struct amd64::TrapFrame* trapFrame = nullptr;
        struct amd64::Context* context = nullptr;
        fs::Inode* cwd = nullptr;
    };

    Process& GetCurrent();

    char* CreateAndMapUserStack(Process& proc);
    void Initialize();
    void Scheduler();
    int Exit(amd64::Syscall& context);
    int Fork(amd64::Syscall& context);
    int WaitPID(amd64::Syscall& context);

} // namespace process
