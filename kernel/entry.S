.text

.global __entry, bootstrap_stack

__entry:
    movq    %rdi, %r8

    // Clear the BSS section
    movq    $__bss_begin, %rdi
    movq    $__bss_end, %rcx
    subq    %rdi, %rcx
    shrq    $3,%rcx
    xorq    %rax, %rax
    rep     stosq

    // Activate our bootstrap stack, restore argument
    movq    $bootstrap_stack, %rsp
    movq    %r8, %rdi
    call    startup

.align        32
.bss
    .space    4096
bootstrap_stack:

