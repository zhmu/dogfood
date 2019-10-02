.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exception10, exception11
.globl exception12, exception13, exception14, exception16, exception17
.globl exception18, exception19
.globl irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
.globl irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
.globl switch_to, trap_return, syscall_handler, syscall_kernel_rsp

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

trap_return:
    RESTORE_REGISTERS
    addq    $0x88, %rsp
    iretq

#define IRQ_HANDLER(n) \
irq ## n: \
    subq    $0x88, %rsp; \
    movq    $n, 0x00(%rsp); \
    jmp do_irq

do_irq:
    SAVE_REGISTERS

    movq    %rsp, %rdi
    call    irq_handler
    jmp     trap_return

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

/* And the IRQ's */
IRQ_HANDLER(0)
IRQ_HANDLER(1)
IRQ_HANDLER(2)
IRQ_HANDLER(3)
IRQ_HANDLER(4)
IRQ_HANDLER(5)
IRQ_HANDLER(6)
IRQ_HANDLER(7)
IRQ_HANDLER(8)
IRQ_HANDLER(9)
IRQ_HANDLER(10)
IRQ_HANDLER(11)
IRQ_HANDLER(12)
IRQ_HANDLER(13)
IRQ_HANDLER(14)
IRQ_HANDLER(15)

switch_to:
    pushq   %rbp
    pushq   %r15
    pushq   %r14
    pushq   %r13
    pushq   %r12
    pushq   %rbx

    movq    %rsp, (%rdi)
    movq    %rsi, %rsp

    popq    %rbx
    popq    %r12
    popq    %r13
    popq    %r14
    popq    %r15
    popq    %rbp
    ret

syscall_handler:
    // Switch to kernel stack
    movq    %rsp, syscall_userland_rsp
    movq    syscall_kernel_rsp, %rsp

    pushq   %rbx
    pushq   %rbp
    pushq   %r11
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    subq    $72, %rsp
    movq    %rax, 0(%rsp)
    movq    %rdi, 8(%rsp)
    movq    %rsi, 16(%rsp)
    movq    %rdx, 24(%rsp)
    movq    %r10, 32(%rsp)
    movq    %r9, 40(%rsp)
    movq    %r8, 48(%rsp)
    movq    %rcx, 56(%rsp)
    movq    syscall_userland_rsp, %rax
    movq    %rax, 64(%rsp)

    sti

    movq    %rsp, %rdi
    call    syscall

    movq    56(%rsp), %rcx
    movq    64(%rsp), %rbx
    movq    %rbx, syscall_userland_rsp
    addq    $72, %rsp

    cli

    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %rbp
    popq    %rbx

    movq    syscall_userland_rsp, %rsp
    sysretq

.data

syscall_userland_rsp:
    .long   0, 0

syscall_kernel_rsp:
    .long   0, 0
