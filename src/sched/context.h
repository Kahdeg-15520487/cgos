#ifndef CONTEXT_H
#define CONTEXT_H

#include "thread.h"

// Perform context switch from 'old' thread to 'new_thread'
// This function saves the current CPU state to old->rsp and
// restores the state from new_thread->rsp.
//
// Must be called with interrupts disabled (cli).
// When the switch completes, execution continues in new_thread
// where it left off (or at its entry point if it's a fresh thread).
extern void context_switch(thread_t *old, thread_t *new_thread);

// Thread entry wrapper - called when a new thread starts executing
// This is an assembly function that sets up the environment and
// calls the thread's entry function with its argument.
extern void thread_entry_wrapper(void);

#endif // CONTEXT_H
