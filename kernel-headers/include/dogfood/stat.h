/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "types.h"

struct stat {
    __dev_t st_dev;
    __ino_t st_ino;
    __mode_t st_mode;
    __nlink_t st_nlink;
    __uid_t st_uid;
    __gid_t st_gid;
    __dev_t st_rdev;
    __off_t st_size;
    __time_t st_atime;
    __time_t st_mtime;
    __time_t st_ctime;
    __blksize_t st_blksize;
    __blkcnt_t st_blocks;
};

#define _IFMT 0170000   /* type of file */
#define _IFDIR 0040000  /* directory */
#define _IFCHR 0020000  /* character special */
#define _IFBLK 0060000  /* block special */
#define _IFREG 0100000  /* regular */
#define _IFLNK 0120000  /* symbolic link */
#define _IFSOCK 0140000 /* socket */
#define _IFIFO 0010000  /* fifo */

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
