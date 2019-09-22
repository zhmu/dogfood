.data

.global initcode, initcode_end

initcode:

        movq $1, %rax
        movq $2, %rdi
        movq $3, %rsi
        movq $4, %rdx
        movq $5, %r10
        movq $6, %r9
        movq $7, %r8
        syscall
1:
        jmp     1b

initcode_end:
