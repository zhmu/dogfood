#include "process.h"
#include "amd64.h"
#include "lib.h"
#include "page_allocator.h"
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

                auto pd = vm::CreateUserlandPageDirectory();
                proc.pageDirectory = vm::VirtualToPhysical(pd);

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
                // Allocate user stack
                {
                    CreateAndMapUserStack(proc);
                    proc.trapFrame->rsp = vm::userland::stackBase + vm::PageSize;
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

    void Initialize()
    {
        {
            auto proc = AllocateProcess();
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
                printf("syscall_kernel_rsp = %lx\n", syscall_kernel_rsp);
                switch_to(&cpu_context, proc.context);
            }
        }
    }

} // namespace process
