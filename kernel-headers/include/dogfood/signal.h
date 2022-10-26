/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "types.h"

typedef __sigset_t sigset_t;
typedef void (*_sig_func_ptr)(int);

#define SIG_ERR ((_sig_func_ptr)-1)
#define SIG_DFL ((_sig_func_ptr)0)
#define SIG_IGN ((_sig_func_ptr)1)

union sigval {
    int sival_int;
    void* sival_ptr;
};

typedef struct {
    int si_signo;
    int si_code;
    int si_errno;
    __pid_t si_pid;
    __uid_t si_uid;
    void* si_addr;
    int si_status;
    long si_band;
    union sigval si_value;
} siginfo_t;

#define SA_NOCHLDSTOP (1 << 0)
#define SA_ONSTACK (1 << 2)
#define SA_RESETHAND (1 << 3)
#define SA_RESTART (1 << 4)
#define SA_SIGINFO (1 << 5)
#define SA_NOCLDWAIT (1 << 6)
#define SA_NODEFER (1 << 7)

struct sigaction {
    void (*sa_handler)(int);
    __sigset_t sa_mask;
    int sa_flags;
    void (*sa_sigaction)(int, siginfo_t*, void*);
    void (*sa_restorer)(void);
};

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPWR    30
#define SIGSYS    31
#define NSIG      (SIGSYS+1)

#define SIG_BLOCK 0
#define SIG_SETMASK 1
#define SIG_UNBLOCK 2

#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192

struct sigaltstack {
    void* ss_sp;
    int ss_flags;
    size_t ss_size;
};

typedef struct sigaltstack stack_t;
