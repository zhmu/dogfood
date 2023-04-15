#ifdef __cplusplus

#include "types.h"
#include "x86_64/amd64.h"
#include <optional>
#include <type_traits>
#include <expected>
#include "error.h"

namespace userspace {
    template<typename T>
    struct Pointer {
        T* p{};

        operator bool() const { return p; }

        std::optional<T> operator*() const {
            T value;
            value = *p; // TODO check if this fails
            return value;
        }

        std::expected<int, error::Code> Set(const T& value) {
            *p = value; // TODO check if this fails
            return 0;
        }

        T* get() { return p; }
        const T* get() const { return p; }
    };
}

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
    auto GetArgument(amd64::TrapFrame& tf)
    {
        using BaseType = std::remove_pointer_t<T>;

        const auto value = detail::GetArgumentValue<N>(tf);
        if constexpr (std::is_pointer_v<T>) {
            const auto p = reinterpret_cast<T>(value);
            return userspace::Pointer<BaseType>{ p };
        } else {
            return static_cast<T>(value);
        }
    }

    inline uint64_t GetNumber(amd64::TrapFrame& tf) { return tf.rax; }

} // namespace syscall
#endif
