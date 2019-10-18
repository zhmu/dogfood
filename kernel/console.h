#pragma once

namespace console
{
    void initialize();
    void put_char(int ch);
    int get_char();
    void OnIRQ();
    int Read(void* buf, int len);

} // namespace console
