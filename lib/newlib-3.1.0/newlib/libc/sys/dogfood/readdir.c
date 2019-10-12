#include <sys/dirent.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

// XXX ext2 direntry
struct DIRENTRY {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[0];
} __attribute__((packed));

struct dirent* readdir(DIR* dirp)
{
    if (dirp->d_done)
        return NULL;

    struct DIRENTRY de;
    if (read(dirp->d_fd, &de, sizeof(de)) < sizeof(de)) {
        dirp->d_done = 1;
        return NULL;
    }

    dirp->d_dirent.d_ino = de.inode;
    if (read(dirp->d_fd, dirp->d_dirent.d_name, de.name_len) != de.name_len) {
        dirp->d_done = 1;
        errno = EIO;
        return NULL;
    }

    return &dirp->d_dirent;
}
