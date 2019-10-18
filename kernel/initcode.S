.data

.global initcode, initcode_end

#include "syscall.h"

initcode:
        movq $SYS_execve, %rax
        leaq init(%rip), %rdi
        leaq argv(%rip), %rsi
        leaq envp(%rip), %rdx
        movq %rdi, (%rsi)
        syscall

        movq $SYS_exit, %rax
        movq $-1, %rdi
        syscall

1:      jmp     1b

init:   .ascii  "/sbin/init\0"
argv:   .quad   0
envp:   .quad   0

initcode_end:
