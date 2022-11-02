#ifndef _SYS_PTRACE_H_
#define _SYS_PTRACE_H_

#include <dogfood/ptrace.h>

#ifdef __cplusplus
extern "C" {
#endif

long ptrace(int request, pid_t pid, void* addr, void* data);

#ifdef __cplusplus
}
#endif

#endif // _SYS_PTRACE_H_
