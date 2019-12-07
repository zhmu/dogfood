#ifdef __cplusplus

#include "types.h"
#include "x86_64/amd64.h"

namespace syscall
{
    template<size_t N>
    uint64_t GetArgument(amd64::TrapFrame&);

    inline uint64_t GetNumber(amd64::TrapFrame& tf) { return tf.rax; }
    template<>
    inline uint64_t GetArgument<1>(amd64::TrapFrame& tf)
    {
        return tf.rdi;
    }
    template<>
    inline uint64_t GetArgument<2>(amd64::TrapFrame& tf)
    {
        return tf.rsi;
    }
    template<>
    inline uint64_t GetArgument<3>(amd64::TrapFrame& tf)
    {
        return tf.rdx;
    }
    template<>
    inline uint64_t GetArgument<4>(amd64::TrapFrame& tf)
    {
        return tf.r10;
    }
    template<>
    inline uint64_t GetArgument<5>(amd64::TrapFrame& tf)
    {
        return tf.r8;
    }
    template<>
    inline uint64_t GetArgument<6>(amd64::TrapFrame& tf)
    {
        return tf.r9;
    }
} // namespace syscall
#endif
