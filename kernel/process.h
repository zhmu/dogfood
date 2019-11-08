#pragma once

#include "types.h"
#include "file.h"
#include "fs.h"

namespace amd64
{
    struct Context;
    struct TrapFrame;
} // namespace amd64

namespace process
{
    enum class State { Unused, Construct, Runnable, Running, Zombie, Sleeping };

    constexpr int maxFiles = 20;

    struct Process {
        State state = State::Unused;
        int pid = -1;
        int ppid = -1;
        int signal = 0;
        Process* parent = nullptr;
        uint64_t heapSize = 0;
        uint64_t heapSizeAllocated = 0;
        uint64_t nextMmapAddress = 0;
        //
        uint64_t pageDirectory = 0;  // physical address
        void* kernelStack = nullptr; // start of kernel stack
        struct amd64::TrapFrame* trapFrame = nullptr;
        struct amd64::Context* context = nullptr;
        uint8_t fpu[512] __attribute__((aligned(16)));
        file::File files[maxFiles];
        fs::Inode* cwd = nullptr;
    };

    Process& GetCurrent();

    char* CreateAndMapUserStack(Process& proc);
    void Initialize();
    void Scheduler();
    int Exit(amd64::TrapFrame& context);
    int Fork(amd64::TrapFrame& context);
    int WaitPID(amd64::TrapFrame& context);
    int Kill(amd64::TrapFrame& context);

} // namespace process
