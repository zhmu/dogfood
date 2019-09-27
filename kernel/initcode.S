.data

.global initcode, initcode_end

#include "syscall.h"

initcode:
        movq $SYS_write, %rax
        movq $1, %rdi   // stdout
        leaq msg(%rip), %rsi // ptr
        movq $12, %rdx  // len
        syscall

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

msg:    .ascii  "Hello world!\0"
init:   .ascii  "/init\0"
argv:   .long   0
envp:   .long   0

initcode_end:
