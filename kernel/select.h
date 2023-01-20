#pragma once

namespace amd64 { struct TrapFrame; }

namespace select{
    long Select(amd64::TrapFrame&);
}
