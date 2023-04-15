#pragma once

#include <expected>
#include "error.h"

namespace amd64 { struct TrapFrame; }

namespace select{
    std::expected<int, error::Code> Select(amd64::TrapFrame&);
}
