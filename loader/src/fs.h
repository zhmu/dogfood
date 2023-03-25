/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "bio.h"
#include <optional>

static constexpr auto ENOSPC = 1;
static constexpr auto EIO = 2;
static constexpr auto ELOOP = 3;
static constexpr auto EEXIST = 4;
static constexpr auto ENOENT = 5;
static constexpr auto EPERM = 6;
static constexpr auto ENOTDIR = 7;
static constexpr auto ENOTEMPTY = 8;
static constexpr auto ENAMETOOLONG = 9;

namespace ext2 {
    struct Inode;
}

namespace fs {
    using InodeNumber = uint32_t;
    using Offset = uint64_t;
    inline constexpr unsigned int MaxPathLength = 256;
    inline constexpr unsigned int MaxDirectoryEntryNameLength = 64;

    struct Inode {
        bio::Device dev = 0;
        InodeNumber inum = 0;
        int refcount = 0;
        bool dirty = false;
        ext2::Inode* ext2inode = nullptr;
    };

    struct DEntry {
        InodeNumber d_ino = 0;
        char d_name[MaxDirectoryEntryNameLength] = {};
    };

    bool Mount(bio::Device dev);

    Inode* iget(bio::Device dev, fs::InodeNumber inum);
    void iput(Inode&);
    void idirty(Inode&);

    std::optional<size_t> Read(fs::Inode& inode, void* dst, fs::Offset offset, size_t count);
    std::optional<size_t> Write(fs::Inode& inode, const void* src, fs::Offset offset, size_t count);

    Inode* namei(const char* path, const bool follow, fs::Inode* parent_inode);
}
