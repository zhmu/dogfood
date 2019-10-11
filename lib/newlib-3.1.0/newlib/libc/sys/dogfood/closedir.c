#include <sys/dirent.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>

int closedir(DIR* dirp)
{
    close(dirp->d_fd);
    free(dirp);
    return 0;
}
