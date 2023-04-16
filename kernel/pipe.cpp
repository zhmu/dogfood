#include "pipe.h"
#include "process.h"
#include "syscall.h"
#include "lib.h"
#include <dogfood/fcntl.h>
#include "debug.h"
#include <numeric>

namespace pipe
{
    namespace
    {
        constexpr debug::Trace<false> Debug;
        constexpr static auto inline PipeBufferSize = 1024;
    }

    result::MaybeInt Pipe::Read(void* buf, int len, const bool nonblock)
    {
        Debug("Pipe::Read ", p_num_readers, " ", p_num_writers, "\n");
        assert(p_num_readers > 0);

        auto state = interrupts::SaveAndDisable();
        if (len == 0) {
            interrupts::Restore(state);
            return 0;
        }
        while (p_deque.empty()) {
            Debug("Pipe::Read blocking, buffer is empty\n");
            if (p_num_writers == 0) {
                interrupts::Restore(state);
                Debug("Pipe::Read no writers\n");
                return 0;
            }
            if (nonblock) {
                interrupts::Restore(state);
                return 0;
            }
            process::Sleep(this);
            Debug("Pipe::Read done blocking\n");
        }

        auto out = static_cast<uint8_t*>(buf);
        size_t total_read = 0;
        for (; !p_deque.empty() && len > 0; --len) {
            out[total_read] = p_deque.front();
            p_deque.pop_front();
            ++total_read;
        }
        process::Wakeup(this);
        interrupts::Restore(state);
        Debug("Pipe::Read -> ", total_read, "\n");
        return total_read;
    }

    result::MaybeInt Pipe::Write(const void* buf, int len)
    {
        Debug("Pipe::Write\n");
        assert(p_num_writers > 0);

        auto state = interrupts::SaveAndDisable();
        if (p_num_readers == 0) {
            interrupts::Restore(state);
            Debug("TODO handle SIGPIPE\n");
            return result::Error(error::Code::BrokenPipe);
        }

        auto in = static_cast<const uint8_t*>(buf);
        size_t total_written = 0;
        for (size_t left = len; left > 0; ) {
            auto chunk_size = std::min(DetermineMaximumWriteSize(), left);
            Debug("Pipe::write len ", len, " left ", left, " chunk_size ", chunk_size, "\n");
            if (chunk_size == 0) {
                Debug("Pipe Write full ", p_num_readers, " ", p_num_writers, "!\n");
                if (p_num_readers == 0) {
                    interrupts::Restore(state);
                    return total_written;
                }
                process::Sleep(this);
                continue;
            }

            for(auto m = 0; m < chunk_size; ++m) {
                p_deque.push_back(*in);
                ++in;
            }
            if (chunk_size > 0)
                process::Wakeup(this);

            total_written += chunk_size;
            left -= chunk_size;
        }

        Debug("Pipe::Write -> ", total_written, " write size, available space ", DetermineMaximumWriteSize(), "\n");
        interrupts::Restore(state);
        return total_written;
    }

    bool Pipe::CanRead()
    {
        auto state = interrupts::SaveAndDisable();
        const auto result = p_num_writers > 0 && !p_deque.empty();
        interrupts::Restore(state);
        return result;
    }

    bool Pipe::CanWrite()
    {
        auto state = interrupts::SaveAndDisable();
        const auto result = p_num_readers > 0 && DetermineMaximumWriteSize() > 0;
        interrupts::Restore(state);
        return result;
    }

    size_t Pipe::DetermineMaximumWriteSize() const
    {
        auto state = interrupts::SaveAndDisable();
        size_t result = PipeBufferSize - p_deque.size();
        interrupts::Restore(state);
        return result;
    }

    result::MaybeInt pipe(amd64::TrapFrame& tf)
    {
        auto fdsPtr = syscall::GetArgument<1, int*>(tf);
        if (!fdsPtr) return result::Error(error::Code::MemoryFault);

        auto& current = process::GetCurrent();
        auto file1 = file::Allocate(current);
        if (!file1) return result::Error(error::Code::NoFile);
        auto file2 = file::Allocate(current);
        if (!file2) {
            file::Free(*file1);
            return result::Error(error::Code::NoFile);
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
