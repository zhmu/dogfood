#pragma once

namespace pic
{
    namespace irq
    {
        inline constexpr int Timer = 0;
        inline constexpr int Keyboard = 1;
        inline constexpr int Slave = 2;
        inline constexpr int IDE = 14;
    } // namespace irq

    void Initialize();
    void Acknowledge();
    void Enable(int irq);
} // namespace pic
