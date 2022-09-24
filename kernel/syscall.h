#ifdef __cplusplus

#include "types.h"
#include "x86_64/amd64.h"
#include <type_traits>

namespace syscall
{
    namespace detail {
        template<size_t N>
        uint64_t GetArgumentValue(amd64::TrapFrame& tf)
        {
            if constexpr (N == 1) {
                return tf.rdi;
            } else if constexpr (N == 2) {
                return tf.rsi;
            } else if constexpr (N == 3) {
                return tf.rdx;
            } else if constexpr (N == 4) {
                return tf.r10;
            } else if constexpr (N == 5) {
                return tf.r8;
            } else if constexpr (N == 6) {
                return tf.r9;
            }
        }
    }

    template<size_t N, typename T = uint64_t>
    T GetArgument(amd64::TrapFrame& tf)
    {
        const auto value = detail::GetArgumentValue<N>(tf);
        if constexpr (std::is_pointer_v<T>) {
            return reinterpret_cast<T>(value);
        } else {
            return static_cast<T>(value);
        }
    }

    inline uint64_t GetNumber(amd64::TrapFrame& tf) { return tf.rax; }

} // namespace syscall
#endif
