#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

int access(const char* filename, int mode)
{
    struct stat sb;
    if (stat(filename, &sb) < 0)
        return -1;
    int ok = 1;
    if ((mode & X_OK) && (sb.st_mode & 0111) == 0)
        ok = 0;
    if ((mode & R_OK) && (sb.st_mode & 0222) == 0)
        ok = 0;
    if ((mode & W_OK) && (sb.st_mode & 0444) == 0)
        ok = 0;
    if (ok) return 0;
    errno = EACCES;
    return -1;
}

