#include "process.h"
#include "x86_64/amd64.h"
#include "lib.h"
#include "page_allocator.h"
#include "syscall.h"
#include "vm.h"
#include <dogfood/errno.h>

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

        void AllocateConsoleFile(Process& proc)
        {
            auto file = file::Allocate(proc);
            assert(file != nullptr);
            file->f_console = true;
        }

        Process* AllocateProcess()
        {
            for (auto& proc : process) {
                if (proc.state != State::Unused)
                    continue;

                proc = Process{};
                proc.state = State::Construct;
                proc.pid = next_pid++;
                proc.cwd = fs::namei("/", true);
                proc.nextMmapAddress = vm::userland::mmapBase;
                AllocateConsoleFile(proc); // stdin
                AllocateConsoleFile(proc); // stdout
                AllocateConsoleFile(proc); // stderr

                auto sp = CreateUserKernelStack(proc);
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

        Process* FindProcessByPID(int pid)
        {
            for (auto& proc : process) {
                if (proc.state != State::Unused && proc.pid == pid)
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
            page_allocator::Free(proc.kernelStack);
            proc.state = State::Unused;
        }

        void Yield()
        {
            amd64::fpu::SaveContext(&current->fpu[0]);
            switch_to(&current->context, cpu_context);
        }
    } // namespace

    Process& GetCurrent() { return *current; }

    char* CreateAndMapUserStack(Process& proc)
    {
        auto ustack = reinterpret_cast<char*>(page_allocator::Allocate());
        assert(ustack != nullptr);
        memset(ustack, 0, vm::PageSize);

        auto pd = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(proc.pageDirectory));
        vm::Map(
            pd, vm::userland::stackBase, vm::PageSize, vm::VirtualToPhysical(ustack),
            vm::Page_P | vm::Page_RW | vm::Page_US);
        return ustack;
    }

    void Sleep(WaitChannel waitChannel, int state)
    {
        assert(interrupts::Save() == 0); // interrupts must be disabled
        if (current == nullptr) {
            interrupts::Enable();
            interrupts::Wait();
            interrupts::Restore(state);
            return;
        }

        current->waitChannel = waitChannel;
        current->state = State::Sleeping;
        interrupts::Restore(state);
        Yield();
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

    int Fork(amd64::TrapFrame& tf)
    {
        auto new_process = AllocateProcess();
        if (new_process == nullptr)
            return -ENOMEM;
        new_process->ppid = current->pid;
        file::CloneTable(*current, *new_process);
        new_process->cwd = current->cwd;
        fs::iref(*new_process->cwd);

        auto current_pd =
            reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(current->pageDirectory));
        auto new_pd = vm::CloneMappings(current_pd);
        new_process->pageDirectory = vm::VirtualToPhysical(new_pd);
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
        return new_process->pid;
    }

    int WaitPID(amd64::TrapFrame& tf)
    {
        auto pid = static_cast<int>(syscall::GetArgument<1>(tf));
        auto stat_loc = reinterpret_cast<int*>(syscall::GetArgument<2>(tf));
        auto options = static_cast<int>(syscall::GetArgument<3>(tf));

        while (true) {
            auto state = interrupts::SaveAndDisable();
            bool have_children = false;
            for (auto& proc : process) {
                if (proc.state == State::Unused || proc.ppid != current->pid)
                    continue;

                have_children = true;
                if (proc.state == State::Zombie) {
                    int pid = proc.pid;
                    *stat_loc = 0; // TODO
                    DestroyZombieProcess(proc);
                    interrupts::Restore(state);
                    return pid;
                }
            }
            if (!have_children) {
                interrupts::Restore(state);
                return -ECHILD;
            }

            Sleep(&process, state);
        }
        // NOTREACHED
    }

    int Kill(amd64::TrapFrame& tf)
    {
        auto pid = static_cast<int>(syscall::GetArgument<1>(tf));
        auto signal = static_cast<int>(syscall::GetArgument<2>(tf));
        if (pid < 0)
            return -EPERM;
        if (signal < 1 || signal > 15)
            return -EINVAL;

        auto proc = FindProcessByPID(pid);
        if (proc == nullptr)
            return -ESRCH;

        printf("kill: pid %d sig %d (own %d)\n", pid, signal, current->pid);
        proc->signal = signal;
        // TODO unblock child if needed
        if (proc == current) {
            Exit(tf);
        }

        return 0;
    }

    int Exit(amd64::TrapFrame& tf)
    {
        if (current->pid == 1)
            panic("init exiting?");

        for (auto& file : current->files) {
            if (file.f_refcount == 0)
                continue;
            file::Free(file);
        }

        auto state = interrupts::SaveAndDisable();
        current->state = State::Zombie;
        interrupts::Restore(state);

        Wakeup(&process);

        Yield();
        panic("Exit() returned");
        // NOTREACHED
    }

    bool MapInode(Process& proc, uint64_t va, uint64_t pteFlags, uint64_t mappingSize, fs::Inode& inode, uint64_t inodeOffset, uint64_t inodeSize)
    {
        auto pml4 = reinterpret_cast<uint64_t*>(vm::PhysicalToVirtual(proc.pageDirectory));
        for (uint64_t offset = 0; offset < mappingSize; offset += vm::PageSize) {
            void* page = page_allocator::Allocate();
            if (page == nullptr)
                return false;
            memset(page, 0, vm::PageSize);
            vm::Map(pml4, va + offset, vm::PageSize, vm::VirtualToPhysical(page), pteFlags);

            const auto readOffset = offset;
            int bytesToRead = vm::PageSize;
            if (readOffset + bytesToRead > inodeSize)
                bytesToRead = inodeSize - readOffset;
#if EXEC_DEBUG
            printf(
                "reading: offset %x, %d bytes -> %p\n", inodeOffset + readOffset, bytesToRead,
                va + offset);
#endif
            if (bytesToRead > 0 &&
                fs::Read(inode, page, inodeOffset + readOffset, bytesToRead) != bytesToRead)
                return false;
        }
        return true;
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
            bool did_switch = false;
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
