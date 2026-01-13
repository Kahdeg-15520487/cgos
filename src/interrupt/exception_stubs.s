.section .text

# Exception handler stubs that save state and call C handlers

# Macro for exception handlers without error code
.macro EXCEPTION_HANDLER num
.global exception_handler_\num
exception_handler_\num:
    # Push dummy error code for stack alignment
    pushq $0
    pushq $\num
    jmp exception_common_stub
.endm

# Macro for exception handlers with error code
.macro EXCEPTION_HANDLER_WITH_ERROR num
.global exception_handler_\num
exception_handler_\num:
    # Error code already pushed by CPU
    pushq $\num
    jmp exception_common_stub_with_error
.endm

# Common exception handler stub (for exceptions without error code)
exception_common_stub:
    # Save all general purpose registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    # Call C handler
    # Exception number is at RSP + 120 (15 registers * 8 bytes)
    # Interrupt frame is at RSP + 128
    movq 120(%rsp), %rdi        # Exception number
    leaq 128(%rsp), %rsi        # Interrupt frame pointer
    call generic_exception_handler
    
    # Restore all general purpose registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    # Remove exception number and dummy error code
    addq $16, %rsp
    
    # Return from interrupt
    iretq

# Common exception handler stub (for exceptions with error code)
exception_common_stub_with_error:
    # Save all general purpose registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    # Call C handler
    # Exception number is at RSP + 120 (15 registers * 8 bytes)
    # Interrupt frame with error is at RSP + 128
    movq 120(%rsp), %rdi        # Exception number
    leaq 128(%rsp), %rsi        # Interrupt frame with error pointer
    call generic_exception_handler_with_error
    
    # Restore all general purpose registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    # Remove exception number (error code remains for iretq)
    addq $8, %rsp
    
    # Return from interrupt (this will also pop the error code)
    iretq

# Define exception handlers
EXCEPTION_HANDLER 0         # Divide Error
EXCEPTION_HANDLER 1         # Debug
EXCEPTION_HANDLER 2         # NMI
EXCEPTION_HANDLER 3         # Breakpoint
EXCEPTION_HANDLER 4         # Overflow
EXCEPTION_HANDLER 5         # Bound Range
EXCEPTION_HANDLER 6         # Invalid Opcode
EXCEPTION_HANDLER 7         # Device Not Available
EXCEPTION_HANDLER_WITH_ERROR 8   # Double Fault
EXCEPTION_HANDLER_WITH_ERROR 10  # Invalid TSS
EXCEPTION_HANDLER_WITH_ERROR 11  # Segment Not Present
EXCEPTION_HANDLER_WITH_ERROR 12  # Stack Fault
EXCEPTION_HANDLER_WITH_ERROR 13  # General Protection
EXCEPTION_HANDLER_WITH_ERROR 14  # Page Fault
EXCEPTION_HANDLER 16        # Floating Point
EXCEPTION_HANDLER_WITH_ERROR 17  # Alignment Check
EXCEPTION_HANDLER 18        # Machine Check
EXCEPTION_HANDLER 19        # SIMD Floating Point

# Mark stack as non-executable to suppress linker warning
.section .note.GNU-stack, "", @progbits
