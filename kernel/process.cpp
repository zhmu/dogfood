#include "process.h"
#include "amd64.h"
#include "lib.h"
#include "page_allocator.h"
#include "vm.h"

extern amd64::TSS kernel_tss;
extern uint64_t* kernel_pagedir;

extern "C" void switch_to(amd64::Context** prevContext, amd64::Context* newContext);
extern "C" void* trap_return;

namespace process
{
    namespace
    {
        inline constexpr size_t maxProcesses = 32;
        Process process[maxProcesses];
        Process* current = nullptr;
        amd64::Context* cpu_context = nullptr;
        int next_pid = 1;

        char* CreateUserKernelStack(Process& proc)
        {
            auto kstack = reinterpret_cast<char*>(page_allocator::Allocate());
            assert(kstack != nullptr);
            proc.kernelStack = kstack;
            return kstack + amd64::PageSize;
        }

        void CreateUserPagedir(Process& proc)
        {
            auto pml4 = reinterpret_cast<uint64_t*>(page_allocator::Allocate());
            assert(pml4 != nullptr);
            memcpy(pml4, kernel_pagedir, amd64::PageSize);
            proc.pageDirectory = vm::VirtualToPhysical(pml4);
        }

        Process* AllocateProcess()
        {
            for (auto& proc : process) {
                if (proc.state != State::Unused)
                    continue;

                proc.state = State::Construct;
                proc.pid = next_pid++;
                auto sp = CreateUserKernelStack(proc);
                // Allocate trap frame
                {
                    sp -= sizeof(amd64::TrapFrame);
                    auto tf = reinterpret_cast<amd64::TrapFrame*>(sp);
                    memset(tf, 0, sizeof(amd64::TrapFrame));
                    tf->rax = 0xab0001;
                    tf->rbx = 0xab0002;
                    tf->rcx = 0xab0003;
                    tf->rdx = 0xab0004;
                    tf->rbp = 0xab0010;
                    tf->rsi = 0xab0005;
                    tf->rdi = 0xab0006;
                    tf->r8 = 0xab0007;
                    tf->r9 = 0xab0008;
                    tf->r10 = 0xab0009;
                    tf->r11 = 0xab000a;
                    tf->r12 = 0xab000b;
                    tf->r13 = 0xab000c;
                    tf->r14 = 0xab000d;
                    tf->r15 = 0xab000e;
                    tf->rip = 0x5555aaaa;
                    tf->cs = static_cast<uint64_t>(amd64::Selector::UserCode) + 3;
                    tf->rflags = 0; // TODO IF
                    tf->rsp = 0xaaaa5555;
                    tf->ss = static_cast<uint64_t>(amd64::Selector::UserData) + 3;
                    proc.trapFrame = tf;
                }
                // Allocate context
                {
                    sp -= sizeof(amd64::Context);
                    auto context = reinterpret_cast<amd64::Context*>(sp);
                    memset(context, 0, sizeof(amd64::Context));
                    context->rdx = 0x19830100;
                    context->r8 = 0x19830111;
                    context->r9 = 0x19830222;
                    context->r10 = 0x19830333;
                    context->r11 = 0x19830444;
                    context->r12 = 0x19830555;
                    context->r13 = 0x19830666;
                    context->r14 = 0x19830777;
                    context->r15 = 0x19830888;
                    context->rip = reinterpret_cast<uint64_t>(&trap_return);
                    proc.context = context;
                }
                return &proc;
            }
            return nullptr;
        }

    } // namespace

    void Initialize()
    {
        {
            auto proc = AllocateProcess();
            assert(proc != nullptr);
            assert(proc->pid == 1);

            proc->state = State::Runnable;
        }
    }

    void Scheduler()
    {
        while (1) {
            for (auto& proc : process) {
                if (proc.state != State::Runnable)
                    continue;

                auto prev = current;
                current = &proc;
                proc.state = State::Running;

                switch_to(&cpu_context, proc.context);
            }
        }
    }

} // namespace process
