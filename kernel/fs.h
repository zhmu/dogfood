#include "types.h"

namespace ext2
{
    struct Inode;
}

namespace fs
{
    using ino_t = uint32_t;

    int Read(ext2::Inode& inode, void* dst, off_t offset, unsigned int count);
}
