#pragma once

#include "types.h"

namespace vm
{
    /*
     * We use the following memory map, [G] means global mapped:
     *
     * From                  To                       Type               Size
     * 0000 0000 0000 0000 - 0000 7fff ffff ffff      Application        127TB
     * ffff 8800 0000 0000 - ffff c7ff ffff ffff  [G] Direct mappings    64TB
     * ffff ffff 8000 0000 - ffff ffff ffff ffff  [G] Kernel text/data   2GB
     */

    /* Convert a physical to a kernel virtual address */
    constexpr uint64_t PhysicalToVirtual(uint64_t addr) { return addr | 0xffff880000000000; }

    constexpr uint64_t VirtualToPhysical(uint64_t addr) { return addr & ~0xffff880000000000; }

} // namespace vm
