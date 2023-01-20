#include "pipe.h"
#include "process.h"
#include "syscall.h"
#include "lib.h"
#include <dogfood/fcntl.h>

namespace pipe
{
    constexpr auto inline DEBUG_PIPE = false;

    namespace {
        template<typename... Args>
        void Debug(Args&&... args)
        {
            if constexpr (DEBUG_PIPE) {
                Print(std::forward<Args>(args)...);
            }
        }
    }

    int Pipe::Read(void* buf, int len, const bool nonblock)
    {
        Debug("Pipe::Read ", p_num_readers, " ", p_num_writers, "\n");
        assert(p_num_readers > 0);

        auto state = interrupts::SaveAndDisable();
        if (p_num_writers == 0) {
            interrupts::Restore(state);
            Debug("Pipe::Read no writers\n");
            return 0;
        }
        if (p_buffer_readpos == p_buffer_writepos) {
            Debug("Pipe::Read blocking\n");
            if (nonblock) {
                interrupts::Restore(state);
                return 0;
            }
            process::Sleep(this, state);
            Debug("Pipe::Read done blocking\n");
        }

        auto out = static_cast<uint8_t*>(buf);
        size_t n = 0;
        for (; len > 0; --len) {
            if (p_buffer_readpos == p_buffer_writepos) break;
            *out = p_buffer[p_buffer_readpos];
            p_buffer_readpos = (p_buffer_readpos + 1) % p_buffer.size();
            ++out, ++n;
        }
        interrupts::Restore(state);
        Debug("Pipe::Read -> ", n, "\n");
        return n;
    }

    int Pipe::Write(const void* buf, int len)
    {
        Debug("Pipe::Write\n");
        assert(p_num_writers > 0);

        auto state = interrupts::SaveAndDisable();
        if (p_num_readers == 0) {
            interrupts::Restore(state);
            Debug("TODO handle SIGPIPE\n");
            return -EPIPE;
        }

        auto in = static_cast<const uint8_t*>(buf);
        size_t n = 0;
        for (; len > 0; --len) {
            p_buffer[p_buffer_writepos] = *in;
            p_buffer_writepos = (p_buffer_writepos + 1) % p_buffer.size();
            ++in, ++n;
        }

        Debug("Pipe::Write -> ", n, "\n");
        if (n > 0)
            process::Wakeup(this);
        interrupts::Restore(state);
        return n;
    }

    int pipe(amd64::TrapFrame& tf)
    {
        auto fdsPtr = syscall::GetArgument<1, int*>(tf);
        if (!fdsPtr) return -EFAULT;

        auto& current = process::GetCurrent();
        auto file1 = file::Allocate(current);
        if (!file1) return -ENFILE;
        auto file2 = file::Allocate(current);
        if (!file2) {
            file::Free(*file1);
            return -ENFILE;
        }
        const auto fd1 = file1 - &current.files[0];
        const auto fd2 = file2 - &current.files[0];
        // TODO handle EFAULT
        fdsPtr.get()[0] = fd1;
        fdsPtr.get()[1] = fd2;

        auto pipe = new Pipe;
        file1->f_pipe = pipe;
        file1->f_flags = O_RDONLY;
        pipe->p_num_readers = 1;
        file2->f_pipe = pipe;
        file2->f_flags = O_WRONLY;
        pipe->p_num_writers = 1;
        Debug("pipe: ", fd1, " ", fd2, "\n");
        return 0;
    }
}
