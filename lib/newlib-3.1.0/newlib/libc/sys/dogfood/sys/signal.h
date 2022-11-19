#ifndef __DOGFOOD_SIGNAL_H__
#define __DOGFOOD_SIGNAL_H__

#include <sys/types.h>
#include <dogfood/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

int kill(pid_t pid, int sig);

int sigaction(int sig, const struct sigaction* act, struct sigaction* oact);
int sigaddset(sigset_t* set, int signo);
int sigdelset(sigset_t* set, int signo);
int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigismember(const sigset_t* set, int signo);
_sig_func_ptr signal(int sig, _sig_func_ptr func);
int sigprocmask(int how, const sigset_t* set, sigset_t* oset);
int sigsuspend(const sigset_t* sigmask);
int sigaltstack(const stack_t* ss, stack_t* old_ss);
int sigpending(sigset_t* set);
int sigwait(const sigset_t* set, int* sig);

#ifdef __cplusplus
}
#endif

#endif // __DOGFOOD_SIGNAL_H__
