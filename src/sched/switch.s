# Context switching assembly for x86-64
# 
# This file contains the low-level context switch routine that saves
# the current thread's state and restores the next thread's state.

# Mark stack as non-executable
.section .note.GNU-stack, "", @progbits

.section .text

.global context_switch
.global thread_entry_wrapper

# =============================================================================
# context_switch(thread_t *old, thread_t *new_thread)
# 
# rdi = old thread (save current state here)
# rsi = new thread (load state from here)
#
# This function:
# 1. Saves callee-saved registers and flags to current stack
# 2. Saves current RSP to old->rsp (offset 24 in thread_t)
# 3. Loads RSP from new_thread->rsp
# 4. Restores callee-saved registers and flags from new stack
# 5. Returns (to the new thread's saved return address)
#
# The saved stack frame looks like this (grows down):
#   [high addr]
#   return address (pushed by call instruction)
#   RFLAGS   <- after pushfq
#   RBP
#   RBX
#   R12
#   R13
#   R14
#   R15      <- RSP points here after all pushes
#   [low addr]
# =============================================================================

context_switch:
    # Save callee-saved registers and flags
    pushfq                      # Save RFLAGS
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    
    # Save current RSP to old thread's rsp field
    # thread_t->rsp is at offset 24 (see thread.h)
    mov %rsp, 24(%rdi)
    
    # Load new thread's RSP
    mov 24(%rsi), %rsp
    
    # Restore callee-saved registers and flags from new thread's stack
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    popfq                       # Restore RFLAGS (note: IF may be 0 if preempted in IRQ)
    sti                         # ALWAYS enable interrupts when resuming a thread
                                # This is critical because context_switch may be called
                                # from within interrupt handlers where IF=0
    
    # Return - this pops the return address from the new thread's stack
    # For a new thread, this will be thread_entry_wrapper
    # For a resumed thread, this will be where it called context_switch from
    ret


# =============================================================================
# thread_entry_wrapper
#
# This is the "return address" for new threads. When a fresh thread is
# context-switched to, context_switch returns here.
#
# At this point:
# - We're running on the new thread's stack
# - The thread_t pointer for current thread is available via thread_current()
# 
# We need to:
# 1. Enable interrupts (they were disabled during context switch)
# 2. Get the current thread's entry function and argument
# 3. Call the entry function
# 4. If the entry function returns, call thread_exit
# =============================================================================

thread_entry_wrapper:
    # Enable interrupts now that we're in the new thread
    sti
    
    # Get current thread pointer
    call thread_current         # Returns thread_t* in %rax
    
    # Load entry function and argument from thread structure
    # thread_t layout:
    #   offset 0:  tid (4 bytes)
    #   offset 4:  state (4 bytes)
    #   offset 8:  kernel_stack_base (8 bytes)
    #   offset 16: kernel_stack_size (8 bytes)
    #   offset 24: rsp (8 bytes)
    #   offset 32: name[32] (32 bytes)
    #   offset 64: entry (8 bytes)      <- function pointer
    #   offset 72: arg (8 bytes)        <- argument
    
    mov 72(%rax), %rdi          # Load arg into first argument register
    mov 64(%rax), %r10          # Load entry function pointer
    
    # Call the thread's entry function
    call *%r10
    
    # If the entry function returns, exit the thread
    call thread_exit
    
    # thread_exit should never return, but just in case...
1:
    hlt
    jmp 1b
