#pragma once

#include "result.h"

namespace amd64 { struct TrapFrame; }

namespace select{
    result::MaybeInt Select(amd64::TrapFrame&);
}
