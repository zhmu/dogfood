#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

char* const child_argv[] = {
    "-sh",
    NULL
};

char* const child_envp[] = {
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
        execve("/bin/sh", child_argv, child_envp);
        abort();
    }
}
