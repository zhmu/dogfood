#include "process.h"
#include "amd64.h"
#include "errno.h"
#include "lib.h"
#include "page_allocator.h"
#include "syscall.h"
#include "vm.h"

extern amd64::TSS kernel_tss;

extern "C" void switch_to(amd64::Context** prevContext, amd64::Context* newContext);
extern "C" void* trap_return;
extern "C" void* initcode;
extern "C" void* initcode_end;
extern "C" uint64_t syscall_kernel_rsp;

namespace process
{
    namespace
    {
        inline constexpr uint64_t initCodeBase = 0x8000000;
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
            return kstack + vm::PageSize;
        }

        Process* AllocateProcess()
        {
            for (auto& proc : process) {
                if (proc.state != State::Unused)
                    continue;

                proc.state = State::Construct;
                proc.pid = next_pid++;

                auto sp = CreateUserKernelStack(proc);
                // Allocate trap frame for trap_return()
                {
                    sp -= sizeof(amd64::TrapFrame);
                    auto tf = reinterpret_cast<amd64::TrapFrame*>(sp);
                    memset(tf, 0, sizeof(amd64::TrapFrame));
                    tf->cs = static_cast<uint64_t>(amd64::Selector::UserCode) + 3;
                    tf->rflags = 0; // TODO IF
                    tf->ss = static_cast<uint64_t>(amd64::Selector::UserData) + 3;
                    proc.trapFrame = tf;
                }
                // Allocate context for switch_to()
                {
                    sp -= sizeof(amd64::Context);
                    auto context = reinterpret_cast<amd64::Context*>(sp);
                    memset(context, 0, sizeof(amd64::Context));
                    context->rip = reinterpret_cast<uint64_t>(&trap_return);
                    proc.context = context;
                }
                return &proc;
            }
            return nullptr;
        }

        Process* CreateProcess()
        {
            auto p = AllocateProcess();
            if (p == nullptr)
                return nullptr;
            auto& proc = *p;

            auto pd = vm::CreateUserlandPageDirectory();
            proc.pageDirectory = vm::VirtualToPhysical(pd);
            // Allocate user stack
            {
                CreateAndMapUserStack(proc);
                proc.trapFrame->rsp = vm::userland::stackBase + vm::PageSize;
            }

            return &proc;
        }

        void DestroyZombieProcess(Process& proc)
        {
            auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(proc.pageDirectory));
            vm::FreeUserlandPageDirectory(pml4);
            if (proc.userStack != nullptr)
                page_allocator::Free(proc.userStack);
            page_allocator::Free(proc.kernelStack);
            proc.state = State::Unused;
        }

    } // namespace

    Process& GetCurrent() { return *current; }

    char* CreateAndMapUserStack(Process& proc)
    {
        auto ustack = reinterpret_cast<char*>(page_allocator::Allocate());
        assert(ustack != nullptr);
        proc.userStack = ustack;

        auto pd = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(proc.pageDirectory));
        vm::Map(
            pd, vm::userland::stackBase, vm::PageSize, vm::VirtualToPhysical(proc.userStack),
            vm::Page_P | vm::Page_RW | vm::Page_US);
        return ustack;
    }

    int Fork(amd64::TrapFrame& tf)
    {
        auto new_process = AllocateProcess();
        if (new_process == nullptr)
            return -ENOMEM;
        new_process->ppid = current->pid;

        auto current_pd =
            reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current->pageDirectory));
        auto new_pd = vm::CloneMappings(current_pd);
        new_process->pageDirectory = vm::VirtualToPhysical(new_pd);
        new_process->state = State::Runnable;

        // We're using trap_return() to yield control back to userland; copy values from syscall
        // frame
        new_process->trapFrame->cs = static_cast<uint64_t>(amd64::Selector::UserCode) + 3;
        new_process->trapFrame->ss = static_cast<uint64_t>(amd64::Selector::UserData) + 3;
        new_process->trapFrame->rflags = 0x202; // XXX
        new_process->trapFrame->rip = tf.rip;
        new_process->trapFrame->rsp = tf.rsp;

        // Restore these registers from the trapframe; the is needed by the ABI
        new_process->trapFrame->rbx = tf.rbx;
        new_process->trapFrame->r12 = tf.r12;
        new_process->trapFrame->r13 = tf.r12;
        new_process->trapFrame->r14 = tf.r14;
        new_process->trapFrame->r15 = tf.r15;
        new_process->trapFrame->rbp = tf.rbp;
        return new_process->pid;
    }

    int WaitPID(amd64::TrapFrame& tf)
    {
        auto pid = static_cast<int>(syscall::GetArgument<1>(tf));
        auto stat_loc = reinterpret_cast<int*>(syscall::GetArgument<2>(tf));
        auto options = static_cast<int>(syscall::GetArgument<3>(tf));

        while (true) {
            bool have_children = false;
            for (auto& proc : process) {
                if (proc.state == State::Unused || proc.ppid != current->pid)
                    continue;

                have_children = true;
                if (proc.state == State::Zombie) {
                    int pid = proc.pid;
                    DestroyZombieProcess(proc);
                    return pid;
                }
            }
            if (!have_children)
                return -ECHILD;

            // TODO implement proper sleep/wakeup mechanism
            current->state = State::Runnable; // XXX
            switch_to(&current->context, cpu_context);
        }
        // NOTREACHED
    }

    int Exit(amd64::TrapFrame& tf)
    {
        if (current->pid == 1)
            panic("init exiting?");

        current->state = State::Zombie;
        switch_to(&current->context, cpu_context);
        for (;;)
            ;
        // NOTREACHED
    }

    void Initialize()
    {
        {
            auto proc = CreateProcess();
            assert(proc != nullptr);
            assert(proc->pid == 1);

            proc->trapFrame->rip = initCodeBase;

            // XXX build some CODE
            auto pd = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(proc->pageDirectory));
            auto code = reinterpret_cast<uint8_t*>(page_allocator::Allocate());
            memcpy(code, &initcode, (uint64_t)&initcode_end - (uint64_t)&initcode);
            vm::Map(
                pd, initCodeBase, vm::PageSize, vm::VirtualToPhysical(code),
                vm::Page_P | vm::Page_RW | vm::Page_US);

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

                amd64::write_cr3(proc.pageDirectory);
                kernel_tss.rsp0 = reinterpret_cast<uint64_t>(
                    reinterpret_cast<char*>(proc.kernelStack) + vm::PageSize);
                syscall_kernel_rsp = reinterpret_cast<uint64_t>(
                    reinterpret_cast<char*>(proc.kernelStack) + vm::PageSize);
                switch_to(&cpu_context, proc.context);
            }
        }
    }

} // namespace process
