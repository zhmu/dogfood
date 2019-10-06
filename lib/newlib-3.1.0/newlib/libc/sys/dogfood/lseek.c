#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

long sys_seek(int, off_t*, int);

off_t lseek(int fd, off_t offset, int whence)
{
    off_t new_offset = offset;
    long status = sys_seek(fd, &new_offset, whence);
    if (status >= 0)
        return new_offset;
  
    errno = EIO;
    return -1;
}
