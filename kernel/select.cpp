#include "select.h"
#include "process.h"
#include "syscall.h"
#include "file.h"
#include "lib.h"
#include <dogfood/select.h>
#include <dogfood/time.h>
#include <dogfood/errno.h>
#include <vector>

namespace select {
    struct SelectItem {
        int fd;
        file::File* file;
    };

    using SelectVector = std::vector<SelectItem>;

    namespace {
        long ConvertFdsPtrToFdSet(userspace::Pointer<fd_set>& p, SelectVector& v)
        {
            if (!p) return 0;

            auto content = *p;
            if (!content) return -EFAULT;

            const auto fds = *content;
            constexpr auto fdsLength = sizeof(fd_set::fds_bits) / sizeof(fd_set::fds_bits[0]);
            for(int n = 0; n < fdsLength * FD_BITS_PER_FDS; ++n) {
                if (!FD_ISSET(n, &fds)) continue;

                const auto file = file::FindByIndex(process::GetCurrent(), n);
                if (file == nullptr) return -EBADF;
                v.push_back({ n, file });
            }

            return 0;
        }

        template<typename Fn>
        int ProcessSelectVector(SelectVector& sv, fd_set& fds, Fn fn)
        {
            int n = 0;
            for(auto& s: sv) {
                if (fn(s.file)) {
                    FD_SET(s.fd, &fds);
                    ++n;
                }
            }
            return n;
        }
    }

    long Select(amd64::TrapFrame& tf)
    {
        const auto nr = syscall::GetArgument<1>(tf);
        auto readfdsPtr = syscall::GetArgument<2, fd_set*>(tf);
        auto writefdsPtr = syscall::GetArgument<3, fd_set*>(tf);
        auto exceptfdsPtr = syscall::GetArgument<4, fd_set*>(tf);
        auto timeoutPtr = syscall::GetArgument<5, timeval*>(tf);

        SelectVector readSv, writeSv, exceptSv;
        if (const auto result = ConvertFdsPtrToFdSet(readfdsPtr, readSv); result != 0)
            return result;
        if (const auto result = ConvertFdsPtrToFdSet(writefdsPtr, writeSv); result != 0)
            return result;
        if (const auto result = ConvertFdsPtrToFdSet(exceptfdsPtr, exceptSv); result != 0)
            return result;

        fd_set readFds, writeFds, exceptFds;
        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        FD_ZERO(&exceptFds);
        int result = 0;
        //Print("Select pid ", process::GetCurrent().pid, " ", readSv.size(), " ", writeSv.size(), " ", exceptSv.size(), "\n");
        while(true) {
            //Print("select loop ", result, "\n");
            result += ProcessSelectVector(readSv, readFds, [](file::File* f) {
                return file::CanRead(*f);
            });
            result += ProcessSelectVector(writeSv, writeFds, [](file::File* f) {
                return file::CanWrite(*f);
            });
            result += ProcessSelectVector(exceptSv, exceptFds, [](file::File* f) {
                return file::HasError(*f);
            });
            if (result != 0) break;

            // TODO we need some kind of sleep mechanism here...
            //process::Yield();
        }

        if (readfdsPtr && !readfdsPtr.Set(readFds)) return -EFAULT;
        if (writefdsPtr && !writefdsPtr.Set(writeFds)) return -EFAULT;
        if (exceptfdsPtr && !exceptfdsPtr.Set(exceptFds)) return -EFAULT;
        return result;
    }
}
