#pragma once

#include "types.h"
#include "file.h"
#include "fs.h"
#include "ptrace.h"
#include "signal.h"
#include "vm.h"

namespace amd64
{
    struct Context;
    struct TrapFrame;
} // namespace amd64

namespace process
{
    enum class State { Unused, Construct, Runnable, Running, Zombie, Sleeping, Stopped };

    constexpr int maxFiles = 20;

    using WaitChannel = void*;
    struct Process {
        State state = State::Unused;
        int pid = -1;
        int umask = 0;
        Process* parent = nullptr;
        WaitChannel waitChannel{};
        //
        uint64_t rsp0 = 0;
        struct amd64::TrapFrame* trapFrame = nullptr;
        struct amd64::Context* context = nullptr;
        uint8_t fpu[512] __attribute__((aligned(16)));
        file::File files[maxFiles];
        fs::Inode* cwd = nullptr;
        vm::VMSpace vmspace;
        signal::State signal;
        ptrace::State ptrace;
    };

    Process& GetCurrent();

    void Initialize();
    void Scheduler();

    void UpdateKernelStackForProcess(Process& proc);

    void Yield();
    void Sleep(WaitChannel, int);
    void Wakeup(WaitChannel);

    int Exit(amd64::TrapFrame& context);
    int Fork(amd64::TrapFrame& context);
    int WaitPID(amd64::TrapFrame& context);
    int ProcInfo(amd64::TrapFrame&);

    Process* FindProcessByPID(int pid);
} // namespace process
