#include "process.h"
#include "x86_64/amd64.h"
#include "lib.h"
#include "exec.h"
#include "page_allocator.h"
#include "syscall.h"
#include "vm.h"
#include <dogfood/errno.h>
#include <dogfood/procinfo.h>
#include <dogfood/wait.h>

extern "C" void switch_to(amd64::Context** prevContext, amd64::Context* newContext);
extern "C" void* trap_return;
extern amd64::TSS kernel_tss;
extern "C" uint64_t syscall_kernel_rsp;

namespace process
{
    namespace
    {
        inline constexpr size_t maxProcesses = 32;
        Process process[maxProcesses];
        Process* current = nullptr;
        amd64::Context* cpu_context = nullptr;
        int next_pid = 1;

        Process* AllocateProcess()
        {
            for (auto& proc : process) {
                if (proc.state != State::Unused)
                    continue;

                proc = Process{};
                proc.state = State::Construct;
                proc.pid = next_pid++;

                vm::InitializeVMSpace(proc.vmspace);
                proc.rsp0 = reinterpret_cast<uint64_t>(proc.vmspace.kernelStack) + vm::PageSize;

                auto sp = static_cast<char*>(proc.vmspace.kernelStack) + vm::PageSize;
                // Allocate trap frame for trap_return()
                {
                    sp -= sizeof(amd64::TrapFrame);
                    auto tf = reinterpret_cast<amd64::TrapFrame*>(sp);
                    memset(tf, 0, sizeof(amd64::TrapFrame));
                    tf->cs = static_cast<uint64_t>(amd64::Selector::UserCode) + 3;
                    tf->rflags = amd64::rflags::IF;
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

        Process* FindNextProcess(int pid)
        {
            Process* next{};
            for(auto& p: process) {
                if (p.pid <= pid) continue;
                if (next == nullptr || next->pid > p.pid) {
                    next = &p;
                }
            }
            return next;
        }


        void DestroyZombieProcess(Process& proc)
        {
            assert(proc.state == State::Zombie);
            vm::DestroyVMSpace(proc.vmspace);
            proc.state = State::Unused;
        }
    } // namespace

    Process& GetCurrent() { return *current; }

    Process* FindProcessByPID(int pid)
    {
        for (auto& proc : process) {
            if (proc.state != State::Unused && proc.pid == pid)
                return &proc;
        }
        return nullptr;
    }

    void Yield()
    {
        amd64::fpu::SaveContext(&current->fpu[0]);
        switch_to(&current->context, cpu_context);
    }

    void Sleep(WaitChannel waitChannel)
    {
        assert(interrupts::Save() == 0); // interrupts must be disabled
        if (current == nullptr) {
            interrupts::Enable();
            interrupts::Wait();
            interrupts::Disable();
        } else {
            current->waitChannel = waitChannel;
            current->state = State::Sleeping;
            interrupts::Enable();
            Yield();
            interrupts::Disable();
        }
    }

    void Wakeup(WaitChannel waitChannel)
    {
        auto state = interrupts::SaveAndDisable();
        for (auto& proc : process) {
            if (proc.state == State::Sleeping && proc.waitChannel == waitChannel)
                proc.state = State::Runnable;
        }
        interrupts::Restore(state);
    }

    result::MaybeInt Fork(amd64::TrapFrame& tf)
    {
        auto new_process = AllocateProcess();
        if (new_process == nullptr)
            return result::Error(error::Code::OutOfSpace);
        new_process->parent = current;
        new_process->umask = current->umask;
        file::CloneTable(*current, *new_process);
        new_process->cwd = fs::ReferenceInode(current->cwd);

        vm::Clone(new_process->vmspace);
        new_process->state = State::Runnable;

        // We're using trap_return() to yield control back to userland; copy values from syscall
        // frame
        new_process->trapFrame->cs = static_cast<uint64_t>(amd64::Selector::UserCode) + 3;
        new_process->trapFrame->ss = static_cast<uint64_t>(amd64::Selector::UserData) + 3;
        new_process->trapFrame->rflags = tf.rflags;
        new_process->trapFrame->rip = tf.rip;
        new_process->trapFrame->rsp = tf.rsp;
        // Interrupts must be enabled in both parent and child
        assert(new_process->trapFrame->rflags & amd64::rflags::IF);

        // Restore these registers from the trapframe; the is needed by the ABI
        new_process->trapFrame->rbx = tf.rbx;
        new_process->trapFrame->r12 = tf.r12;
        new_process->trapFrame->r13 = tf.r12;
        new_process->trapFrame->r14 = tf.r14;
        new_process->trapFrame->r15 = tf.r15;
        new_process->trapFrame->rbp = tf.rbp;
        assert(((new_process->trapFrame->rsp - 8) & 0xf) == 0);
        return new_process->pid;
    }

    result::MaybeInt WaitPID(amd64::TrapFrame& tf)
    {
        const auto pid = syscall::GetArgument<1, int>(tf);
        auto statLocPtr = syscall::GetArgument<2, int*>(tf);
        const auto options = syscall::GetArgument<3, int>(tf);

        while (true) {
            auto state = interrupts::SaveAndDisable();
            bool have_children = false;
            for (auto& proc : process) {
                if (proc.state == State::Unused || proc.parent != current)
                    continue;

                have_children = true;
                if (proc.state == State::Stopped) {
                    if (proc.ptrace.signal == 0) continue;
                    if ((options & WUNTRACED) == 0 && !proc.ptrace.traced) continue;
                    const auto setOkay = !statLocPtr || statLocPtr.Set(W_MAKE(W_STATUS_STOPPED, proc.ptrace.signal));
                    proc.ptrace.signal = 0;
                    interrupts::Restore(state);
                    if (setOkay) return pid;
                    return result::Error(error::Code::MemoryFault);
                }

                if (proc.state == State::Zombie) {
                    const auto pid = proc.pid;
                    const auto setOkay = !statLocPtr || statLocPtr.Set(W_MAKE(W_STATUS_EXITED, 0));
                    DestroyZombieProcess(proc);
                    interrupts::Restore(state);
                    if (setOkay) return pid;
                    return result::Error(error::Code::MemoryFault);
                }
            }
            if (!have_children) {
                interrupts::Restore(state);
                return result::Error(error::Code::NoChild);
            }

            if (options & WNOHANG) {
                interrupts::Restore(state);
                return 0;
            }

            Sleep(&process);
        }
        // NOTREACHED
    }

    result::MaybeInt Exit(amd64::TrapFrame& tf)
    {
        if (current->pid == 1)
            panic("init exiting?");

        for (auto& file : current->files) {
            file::Free(file);
        }

        vm::FreeMappings(current->vmspace);

        auto state = interrupts::SaveAndDisable();
        current->state = State::Zombie;
        for (auto& proc : process) {
            if (proc.parent == current) {
                proc.parent = &process[0]; // init
                assert(proc.parent->pid == 1);
            }
        }
        interrupts::Restore(state);

        Wakeup(&process);

        Yield();
        panic("Exit() returned");
    }

    result::MaybeInt ProcInfo(amd64::TrapFrame& tf)
    {
        const auto pid = syscall::GetArgument<1, int>(tf);
        const auto pi_size = syscall::GetArgument<2, size_t>(tf);
        auto piPtr = syscall::GetArgument<3, PROCINFO*>(tf);

        if (pi_size != sizeof(PROCINFO)) return result::Error(error::Code::ResultTooLarge);

        auto proc = FindProcessByPID(pid);
        if (proc == nullptr) return result::Error(error::Code::NotFound);

        auto next = FindNextProcess(pid);
        PROCINFO pi{};
        pi.next_pid = next ? next->pid : 0;
        pi.state = [&proc]() {
            switch(proc->state) {
                case State::Construct: return PROCINFO_STATE_CONSTRUCT;
                case State::Runnable: return PROCINFO_STATE_RUNNABLE;
                case State::Running: return PROCINFO_STATE_RUNNING;
                case State::Zombie: return PROCINFO_STATE_ZOMBIE;
                case State::Sleeping: return PROCINFO_STATE_SLEEPING;
            }
            return PROCINFO_STATE_UNKNOWN;
        }();

        if (auto argv0 = exec::ExtractArgv0(proc->vmspace, PROCINFO_MAX_NAME_LEN); argv0) {
            memcpy(pi.name, argv0, strlen(argv0) + 1 /* terminating \0 */ );
        }

        return piPtr.Set(pi);
    }

    void Initialize()
    {
        auto init = AllocateProcess();
        assert(init != nullptr);
        assert(init->pid == 1);

        auto cwd = fs::namei("/", fs::Follow::Yes, {});
        assert(cwd);
        init->cwd = std::move(*cwd);
        file::AllocateConsole(*init); // 0, stdin
        file::AllocateConsole(*init); // 1, stdout
        file::AllocateConsole(*init); // 2, stderr
        vm::SetupForInitProcess(init->vmspace, *init->trapFrame);

        init->state = State::Runnable;
    }

    void UpdateKernelStackForProcess(Process& proc)
    {
        kernel_tss.rsp0 = proc.rsp0;
        syscall_kernel_rsp = proc.rsp0;
    }

    void Scheduler()
    {
        while (1) {
            bool did_switch = false;
            for (auto& proc : process) {
                if (proc.state != State::Runnable)
                    continue;

                auto prev = current;
                current = &proc;
                proc.state = State::Running;

                vm::Activate(proc.vmspace);
                UpdateKernelStackForProcess(proc);
                if (prev && prev != current) {
                    amd64::fpu::SaveContext(&prev->fpu[0]);
                    amd64::fpu::RestoreContext(&current->fpu[0]);
                }
                did_switch = true;
                switch_to(&cpu_context, proc.context);
            }

            if (!did_switch) {
                interrupts::Wait();
            }
        }
    }

} // namespace process
