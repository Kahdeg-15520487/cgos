.section .text

# Macro to create interrupt service routines without error codes
.macro ISR_NOERRCODE num
    .global isr\num
    .type isr\num, @function
    isr\num:
        pushq $0                    # Push dummy error code
        pushq $\num                 # Push interrupt number
        jmp isr_common_stub
.endm

# Macro to create interrupt service routines with error codes
.macro ISR_ERRCODE num
    .global isr\num
    .type isr\num, @function
    isr\num:
        pushq $\num                 # Push interrupt number
        jmp isr_common_stub
.endm

# Macro to create IRQ handlers
.macro IRQ num, irq_num
    .global irq\num
    .type irq\num, @function
    irq\num:
        pushq $0                    # Push dummy error code
        pushq $\irq_num             # Push IRQ number
        jmp isr_common_stub
.endm

# Exception handlers (0-31)
ISR_NOERRCODE 0     # Division by zero
ISR_NOERRCODE 1     # Debug
ISR_NOERRCODE 2     # NMI
ISR_NOERRCODE 3     # Breakpoint
ISR_NOERRCODE 4     # Overflow
ISR_NOERRCODE 5     # Bound range exceeded
ISR_NOERRCODE 6     # Invalid opcode
ISR_NOERRCODE 7     # Device not available
ISR_ERRCODE   8     # Double fault
ISR_ERRCODE   10    # Invalid TSS
ISR_ERRCODE   11    # Segment not present
ISR_ERRCODE   12    # Stack segment fault
ISR_ERRCODE   13    # General protection fault
ISR_ERRCODE   14    # Page fault
ISR_NOERRCODE 16    # x87 FPU error
ISR_ERRCODE   17    # Alignment check
ISR_NOERRCODE 18    # Machine check
ISR_NOERRCODE 19    # SIMD FP exception

# IRQ handlers (32-47)
IRQ 0,  32          # Timer
IRQ 1,  33          # Keyboard
IRQ 2,  34          # Cascade
IRQ 3,  35          # COM2
IRQ 4,  36          # COM1
IRQ 5,  37          # LPT2
IRQ 6,  38          # Floppy
IRQ 7,  39          # LPT1
IRQ 8,  40          # RTC
IRQ 9,  41          # Free
IRQ 10, 42          # Free
IRQ 11, 43          # Free
IRQ 12, 44          # Mouse
IRQ 13, 45          # FPU
IRQ 14, 46          # Primary ATA
IRQ 15, 47          # Secondary ATA

# Common interrupt handler stub
isr_common_stub:
    # Save all registers
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
    
    # Save segment registers (not really needed in long mode, but for completeness)
    mov %ds, %ax
    pushq %rax
    mov %es, %ax
    pushq %rax
    mov %fs, %ax
    pushq %rax
    mov %gs, %ax
    pushq %rax
    
    # Load kernel data segment
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    # Align stack to 16 bytes (System V ABI requirement)
    mov %rsp, %rbp
    and $0xFFFFFFFFFFFFFFF0, %rsp
    
    # Call the common interrupt handler in C
    # The interrupt frame is already on the stack
    mov %rbp, %rdi          # Pass stack pointer as argument
    call interrupt_handler_common
    
    # Restore stack pointer
    mov %rbp, %rsp
    
    # Restore segment registers
    popq %rax
    mov %ax, %gs
    popq %rax
    mov %ax, %fs
    popq %rax
    mov %ax, %es
    popq %rax
    mov %ax, %ds
    
    # Restore all registers
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
    
    # Remove interrupt number and error code from stack
    add $16, %rsp
    
    # Return from interrupt
    iretq

# IDT flush function
.global idt_flush
.type idt_flush, @function
idt_flush:
    lidt (%rdi)
    ret

# Mark stack as non-executable
.section .note.GNU-stack,"",@progbits
