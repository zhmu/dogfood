#include <stdlib.h>

extern int main(int, char**, char**);

extern char** environ;

extern void _fini(void);
extern void _init(void);

char* __progname;

void _start(unsigned long* sp)
{
    int argc = *sp++;
    char** argv = (char**)sp;
    sp += argc + 1 /* null */;
    environ = (char**)sp;
    atexit(_fini);
    __progname = argv[0];
    _init();

    exit(main(argc, argv, environ));
}
