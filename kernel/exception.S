.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exception10, exception11
.globl exception12, exception13, exception14, exception16, exception17
.globl exception18, exception19

#define SAVE_REGISTERS \
    movq    %rax, 0x08(%rsp); \
    movq    %rbx, 0x10(%rsp); \
    movq    %rcx, 0x18(%rsp); \
    movq    %rdx, 0x20(%rsp); \
    movq    %rbp, 0x28(%rsp); \
    movq    %rsi, 0x30(%rsp); \
    movq    %rdi, 0x38(%rsp); \
    movq    %r8,  0x40(%rsp); \
    movq    %r9,  0x48(%rsp); \
    movq    %r10, 0x50(%rsp); \
    movq    %r11, 0x58(%rsp); \
    movq    %r12, 0x60(%rsp); \
    movq    %r13, 0x68(%rsp); \
    movq    %r14, 0x70(%rsp); \
    movq    %r15, 0x78(%rsp);

#define RESTORE_REGISTERS \
    movq    0x08(%rsp), %rax; \
    movq    0x10(%rsp), %rbx; \
    movq    0x18(%rsp), %rcx; \
    movq    0x20(%rsp), %rdx; \
    movq    0x28(%rsp), %rbp; \
    movq    0x30(%rsp), %rsi; \
    movq    0x38(%rsp), %rdi; \
    movq    0x40(%rsp), %r8; \
    movq    0x48(%rsp), %r9; \
    movq    0x50(%rsp), %r10; \
    movq    0x58(%rsp), %r11; \
    movq    0x60(%rsp), %r12; \
    movq    0x68(%rsp), %r13; \
    movq    0x70(%rsp), %r14; \
    movq    0x78(%rsp), %r15;

#define EXCEPTION_WITHOUT_ERRCODE(n) \
exception ## n: \
    subq    $0x88, %rsp; \
    movq    $n, 0x00(%rsp); \
    movq    $0, 0x80(%rsp); \
    jmp do_exception

#define EXCEPTION_WITH_ERRCODE(n) \
exception ## n: \
    subq    $0x80, %rsp; \
    movq    $n, 0x00(%rsp); \
    jmp do_exception

do_exception:
    // Note: we don't do any swapgs here (no userland yet)
    SAVE_REGISTERS

    movq    %rsp, %rdi
    call    exception

    RESTORE_REGISTERS
    addq    $0x88, %rsp
    iretq

/* Now we just need to list the exception handlers */
EXCEPTION_WITHOUT_ERRCODE(0)
EXCEPTION_WITHOUT_ERRCODE(1)
EXCEPTION_WITHOUT_ERRCODE(2) /* NMI */
EXCEPTION_WITHOUT_ERRCODE(3)
EXCEPTION_WITHOUT_ERRCODE(4)
EXCEPTION_WITHOUT_ERRCODE(5)
EXCEPTION_WITHOUT_ERRCODE(6)
EXCEPTION_WITHOUT_ERRCODE(7)
EXCEPTION_WITH_ERRCODE(8)
EXCEPTION_WITHOUT_ERRCODE(9)
EXCEPTION_WITH_ERRCODE(10)
EXCEPTION_WITH_ERRCODE(11)
EXCEPTION_WITH_ERRCODE(12)
EXCEPTION_WITH_ERRCODE(13)
EXCEPTION_WITH_ERRCODE(14)
EXCEPTION_WITHOUT_ERRCODE(16)
EXCEPTION_WITH_ERRCODE(17)
EXCEPTION_WITHOUT_ERRCODE(18)
EXCEPTION_WITHOUT_ERRCODE(19)
