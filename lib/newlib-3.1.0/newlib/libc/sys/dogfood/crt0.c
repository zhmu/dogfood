#include <stdlib.h>

extern int main(int, char**, char**);

extern char** environ;

extern void __libc_init_array();

char* __progname;

void _start(unsigned long* sp)
{
    int argc = *sp++;
    char** argv = (char**)sp;
    sp += argc + 1 /* null */;
    environ = (char**)sp;
    __progname = argv[0];
    __libc_init_array();

    exit(main(argc, argv, environ));
}
