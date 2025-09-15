.section .text

# void gdt_flush(uint64_t gdt_ptr)
# Load the GDT and update segment registers
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    # Save registers
    push %rbp
    mov %rsp, %rbp
    
    # Load the GDT
    lgdt (%rdi)
    
    # Reload code segment by doing a far jump
    # We use a trick here: push the new CS and the address to jump to,
    # then use lretq to perform the far jump
    pushq $0x08          # New CS (kernel code segment)
    leaq .reload_cs(%rip), %rax
    pushq %rax
    lretq

.reload_cs:
    # Reload data segment registers
    mov $0x10, %ax       # Kernel data segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    # Restore registers and return
    pop %rbp
    ret

# Mark stack as non-executable
.section .note.GNU-stack,"",@progbits