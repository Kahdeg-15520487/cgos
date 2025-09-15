.section .text

# void vmm_load_page_directory(uintptr_t pml4_physical)
# Load the PML4 page directory and enable paging
.global vmm_load_page_directory
.type vmm_load_page_directory, @function
vmm_load_page_directory:
    # Save registers
    push %rbp
    mov %rsp, %rbp
    push %rax
    push %rbx
    
    # Load PML4 address into CR3
    mov %rdi, %cr3
    
    # Enable paging (set PG bit in CR0)
    mov %cr0, %rax
    or $0x80000000, %eax    # Set bit 31 (PG) - use 32-bit operand
    mov %rax, %cr0
    
    # Flush TLB by reloading CR3
    mov %cr3, %rbx
    mov %rbx, %cr3
    
    # Restore registers and return
    pop %rbx
    pop %rax
    pop %rbp
    ret

# Mark stack as non-executable
.section .note.GNU-stack,"",@progbits
