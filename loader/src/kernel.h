#pragma once

#include <cstdint>
#include <span>

namespace fs { struct Inode; }

namespace kernel
{
bool Load(fs::Inode& inode, std::span<uint8_t> headers);
void Execute();
}
