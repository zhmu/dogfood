#include "types.h"

typedef short dev_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;
typedef uint32_t id_t;
typedef unsigned short ino_t;
typedef uint32_t mode_t;
typedef long off_t;
typedef unsigned short nlink_t;
typedef long blksize_t;
typedef long blkcnt_t;
typedef long time_t;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    long st_spare1;
    time_t st_mtime;
    long st_spare2;
    time_t st_ctime;
    long st_spare3;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    long st_spare4[2];
};

#define _IFMT 0170000   /* type of file */
#define _IFDIR 0040000  /* directory */
#define _IFCHR 0020000  /* character special */
#define _IFBLK 0060000  /* block special */
#define _IFREG 0100000  /* regular */
#define _IFLNK 0120000  /* symbolic link */
#define _IFSOCK 0140000 /* socket */
#define _IFIFO 0010000  /* fifo */

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFL 2
#define F_GETFL 3 /* Get file flags */

#define O_RDONLY    0       /* +1 == FREAD */
#define O_WRONLY    1       /* +1 == FWRITE */
#define O_RDWR      2       /* +1 == FREAD|FWRITE */
#define O_DIRECTORY 0x200000
#define O_CREAT     0x0200
#define O_TRUNC     0x0400
#define O_EXCL      0x0800

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
