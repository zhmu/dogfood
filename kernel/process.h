#pragma once

#include "types.h"
#include "file.h"
#include "fs.h"
#include "vm.h"

namespace amd64
{
    struct Context;
    struct TrapFrame;
} // namespace amd64

namespace process
{
    enum class State { Unused, Construct, Runnable, Running, Zombie, Sleeping };

    constexpr int maxFiles = 20;

    using WaitChannel = void*;
    struct Process {
        State state = State::Unused;
        int pid = -1;
        int ppid = -1;
        int signal = 0;
        Process* parent = nullptr;
        WaitChannel waitChannel{};
        //
        struct amd64::TrapFrame* trapFrame = nullptr;
        struct amd64::Context* context = nullptr;
        uint8_t fpu[512] __attribute__((aligned(16)));
        file::File files[maxFiles];
        fs::Inode* cwd = nullptr;
        vm::VMSpace vmspace;
    };

    Process& GetCurrent();

    void Initialize();
    void Scheduler();

    void Sleep(WaitChannel, int);
    void Wakeup(WaitChannel);

    int Exit(amd64::TrapFrame& context);
    int Fork(amd64::TrapFrame& context);
    int WaitPID(amd64::TrapFrame& context);
    int Kill(amd64::TrapFrame& context);
    int ProcInfo(amd64::TrapFrame&);

} // namespace process
