#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dogfood/procinfo.h>

extern long _SYS_procinfo(long pid, long pi_size, struct PROCINFO*);

int main(int argc, char* argv[])
{
    for (int pid = 1; pid != 0; ) {
        struct PROCINFO pi;
        long r = _SYS_procinfo(pid, sizeof(pi), &pi);
        if (r < 0) {
            errno = r & 0x1ff;
            if (errno == ERANGE) {
                fprintf(stderr, "sizeof(PROCINFO) mismatch - recompile kernel and userland\n");
            } else {
                fprintf(stderr, "procinfo: %s\n", strerror(errno));
            }
            return -1;
        }
        printf("%5d %c %s\n", pid, pi.state, pi.name);
        pid = pi.next_pid;
    }
    return 0;
}
