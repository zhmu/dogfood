#pragma once

namespace bio
{
    struct Buffer;
}

namespace ide
{
    void Initialize();
    void PerformIO(bio::Buffer&);
    void OnIRQ();
} // namespace ide
