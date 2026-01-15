# GDT assembly helpers for x86-64
# This file contains the assembly routines needed to load the GDT
# and properly reload segment registers in long mode.

# Mark stack as non-executable
.section .note.GNU-stack, "", @progbits

.section .text
.global gdt_load

# void gdt_load(gdt_ptr_t *ptr, uint16_t code_selector, uint16_t data_selector)
# rdi = pointer to GDT descriptor (limit + base)
# rsi = kernel code segment selector (e.g., 0x08)
# rdx = kernel data segment selector (e.g., 0x10)
#
# In long mode, we need to:
# 1. Load the new GDT with lgdt
# 2. Reload CS by doing a far return
# 3. Reload DS, ES, SS, FS, GS with the data selector

gdt_load:
    # Load the GDT
    lgdt (%rdi)
    
    # Reload data segment registers
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %ss
    
    # FS and GS are typically 0 in long mode (or used for thread-local storage)
    xor %ax, %ax
    mov %ax, %fs
    mov %ax, %gs
    
    # To reload CS, we need to do a far return
    # Push the new CS and the return address, then do retfq
    
    # Get return address
    pop %rax
    
    # Push new CS (from rsi, widened to 64-bit for the far return)
    push %rsi
    
    # Push return address
    push %rax
    
    # Far return - pops RIP and CS
    retfq
