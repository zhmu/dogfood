#include "fs.h"
#include "bio.h"
#include "ext2.h"
#include "lib.h"

namespace fs {

namespace {
    constexpr inline int RootDevice = 0;
}

int Read(ext2::Inode& inode, void* dst, off_t offset, unsigned int count)
{
    if (offset > inode.i_size || offset + count < offset)
        return -1;
    if (offset + count > inode.i_size) {
        count = inode.i_size - offset;
    }

    auto d = reinterpret_cast<char*>(dst);
    while(count > 0) {
        int chunkLen = count;
        if (offset % bio::BlockSize) {
            chunkLen = bio::BlockSize - (offset % bio::BlockSize);
            if (chunkLen > count) chunkLen = count;
        }
        if (chunkLen > bio::BlockSize)
            chunkLen = bio::BlockSize;

        //printf("{offset %d count %d chunkLen %d inodeblock %d}\n", offset,count, chunkLen, offset / bio::BlockSize);
        const uint32_t blockNr = ext2::bmap(inode, offset / bio::BlockSize);
        auto& buf = bio::bread(RootDevice, blockNr);
        memcpy(d, buf.data + (offset % bio::BlockSize), chunkLen);
        bio::brelse(buf);

        d += chunkLen;
        offset += chunkLen;
        count -= chunkLen;
    }
    return d - reinterpret_cast<char*>(dst);
}

}
