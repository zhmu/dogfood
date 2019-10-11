#include <sys/dirent.h>
#include <dirent.h>
#include <unistd.h>

void rewinddir(DIR* dirp)
{
    dirp->d_done = 0;
    lseek(dirp->d_fd, 0, SEEK_SET);
}
