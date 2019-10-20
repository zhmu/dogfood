#include <sys/dirent.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

DIR* opendir(const char* name)
{
    int fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        return NULL;

    DIR* dir = malloc(sizeof(*dir));
    if (dir == NULL) {
        close(fd);
        return NULL;
    }

    dir->d_fd = fd;
    dir->d_done = 0;
    return dir;
}
