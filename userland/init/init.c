#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

char* const argv[] = {
    "-sh",
    NULL
};

char* const envp[] = {
    "USER=root",
    "PATH=/bin:/usr/bin:/usr/sbin",
    NULL
};

int main(int argc, char* argv[])
{
    for(;;) {
        int pid = fork();
        if (pid != 0) {
            waitpid(-1, (int*)0,0);
            fprintf(stderr, "%s: child died, will restart\n", argv[0]);
            continue;
        }
        execve("/bin/sh", argv, envp);
        fprintf(stderr, "%s: exec() failed, aborting\n", argv[0]);
        abort();
    }
}
