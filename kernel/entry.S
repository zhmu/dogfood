.text

.global __entry, bootstrap_stack

__entry:
    movq    $bootstrap_stack, %rsp

    call    startup

.align        32
.bss
    .space    4096
bootstrap_stack:

