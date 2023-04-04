#pragma once

namespace debug
{
    template<bool Enabled>
    struct Trace
    {
        template<typename... Args>
        void operator()(Args&&... args) const
        {
            if constexpr (Enabled) {
                Print(std::forward<Args>(args)...);
            }
        }
    };
}
