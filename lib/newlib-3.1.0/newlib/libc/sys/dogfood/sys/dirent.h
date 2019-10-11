#ifndef __SYS_DIRENT_H__
#define __SYS_DIRENT_H__

#define MAXNAMELEN 255

struct dirent {
    int d_ino;
    char d_name[MAXNAMELEN + 1];
};

typedef struct {
    int d_fd;
    int d_done;
    struct dirent d_dirent;
} DIR;

#endif // __SYS_DIRENT_H__
