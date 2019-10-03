#pragma once

namespace amd64
{
    struct TrapFrame;
}

int exec(amd64::TrapFrame&);
