#include "../../../../../../kernel/syscall.h"

.text
.globl  _exit, open, close, read, write, fork, waitpid, fstat, sbrk, getpid
.globl  sys_seek

do_syscall:
    movq    %rcx, %r10
    syscall
    ret

_exit:
    movq    $SYS_exit,%rax
    syscall
    ud2

open:
    movq    $SYS_open,%rax
    jmp     do_syscall

close:
    movq    $SYS_close,%rax
    jmp     do_syscall

read:
    movq    $SYS_read,%rax
    jmp     do_syscall

write:
    movq    $SYS_write,%rax
    jmp     do_syscall

fork:
    movq    $SYS_clone,%rax
    xorq    %rdi,%rdi
    jmp     do_syscall

waitpid:
    movq    $SYS_waitpid,%rax
    jmp     do_syscall

fstat:
    movq    $SYS_waitpid,%rax
    jmp     do_syscall

sbrk:
    movq    $SYS_sbrk,%rax
    jmp     do_syscall

getpid:
    movq    $SYS_getpid,%rax
    jmp     do_syscall

sys_seek:
    movq    $SYS_seek,%rax
    jmp     do_syscall
