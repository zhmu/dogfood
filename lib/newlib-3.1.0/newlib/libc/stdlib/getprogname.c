#include <stdlib.h>

extern char* __progname;

const char* getprogname()
{
    return __progname;
}
