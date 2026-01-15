#include "thread.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../debug/debug.h"
#include "../gdt/gdt.h"
#include <string.h>

// Thread table - all threads in the system
static thread_t *all_threads[MAX_THREADS];
static uint32_t next_tid = 1;

// Forward declaration of scheduler functions
extern void scheduler_add(thread_t *thread);
extern thread_t *thread_current(void);  // Defined in scheduler.c

// Calculate time slice based on priority
// Higher priority = longer time slice
static uint32_t calculate_time_slice(uint8_t priority) {
    // Priority 0 (realtime): 25ms
    // Priority 3 (normal): 15ms  
    // Priority 6 (idle): 5ms
    return TIME_SLICE_BASE_MS + (PRIORITY_LEVELS - priority) * 3;
}

// Initialize a thread's kernel stack for first context switch
// Stack layout (grows down):
//   [top - 8]  : entry function address (will be "returned" to)
//   [top - 16] : initial RFLAGS (interrupts enabled)
//   [top - 24] : RBP (0)
//   [top - 32] : RBX (0)
//   [top - 40] : R12 (0)
//   [top - 48] : R13 (0)
//   [top - 56] : R14 (0)
//   [top - 64] : R15 (0)
//   ^ This is where RSP will point after context_switch pops registers

static void thread_init_stack(thread_t *thread) {
    // Start at top of stack
    uint64_t *stack = (uint64_t *)(thread->kernel_stack_base + thread->kernel_stack_size);
    
    // Push thread_entry_wrapper address (where context_switch will "return" to)
    extern void thread_entry_wrapper(void);
    *(--stack) = (uint64_t)thread_entry_wrapper;
    
    // Push initial RFLAGS with interrupts enabled (IF = 1, bit 9)
    *(--stack) = 0x202;  // IF=1, reserved bit 1 always set
    
    // Push callee-saved registers (all zero for fresh thread)
    *(--stack) = 0;  // RBP
    *(--stack) = 0;  // RBX
    *(--stack) = 0;  // R12
    *(--stack) = 0;  // R13
    *(--stack) = 0;  // R14
    *(--stack) = 0;  // R15
    
    // Save the initial RSP
    thread->rsp = (uint64_t)stack;
}

thread_t *thread_create_priority(const char *name, thread_entry_t entry, 
                                  void *arg, uint8_t priority) {
    if (priority >= PRIORITY_LEVELS) {
        priority = PRIORITY_NORMAL;
    }
    
    // Allocate thread structure
    // For simplicity, allocate from physical pages and use HHDM
    void *thread_page = physical_alloc_page();
    if (!thread_page) {
        DEBUG_ERROR("thread_create: failed to allocate thread structure\n");
        return NULL;
    }
    thread_t *thread = (thread_t *)PHYS_TO_HHDM((uint64_t)thread_page);
    memset(thread, 0, sizeof(thread_t));
    
    // Allocate kernel stack (2 pages = 8KB)
    void *stack_pages = physical_alloc_pages(KERNEL_STACK_SIZE / 4096);
    if (!stack_pages) {
        DEBUG_ERROR("thread_create: failed to allocate kernel stack\n");
        physical_free_page(thread_page);
        return NULL;
    }
    
    // Assign TID
    thread->tid = next_tid++;
    
    // Copy name (simple implementation to avoid strncpy dependency)
    size_t i;
    for (i = 0; i < sizeof(thread->name) - 1 && name[i] != '\0'; i++) {
        thread->name[i] = name[i];
    }
    thread->name[i] = '\0';
    
    // Set entry point
    thread->entry = entry;
    thread->arg = arg;
    
    // Set up stack (convert to virtual address via HHDM)
    thread->kernel_stack_base = (uint64_t)PHYS_TO_HHDM((uint64_t)stack_pages);
    thread->kernel_stack_size = KERNEL_STACK_SIZE;
    
    // Set priority
    thread->priority = priority;
    thread->base_priority = priority;
    thread->time_slice_length = calculate_time_slice(priority);
    thread->time_slice = thread->time_slice_length;
    
    // Initialize CPU usage tracking
    thread->history_index = 0;
    thread->avg_cpu_usage = 50;  // Assume 50% initially
    memset(thread->cpu_usage_history, 50, CPU_HISTORY_SAMPLES);
    
    // Initialize stack for first context switch
    thread_init_stack(thread);
    
    // Add to global thread table
    for (int i = 0; i < MAX_THREADS; i++) {
        if (all_threads[i] == NULL) {
            all_threads[i] = thread;
            break;
        }
    }
    
    thread->state = THREAD_STATE_CREATED;
    
    DEBUG_INFO("Created thread '%s' (TID=%d, priority=%d, stack=0x%lx)\n",
               thread->name, thread->tid, thread->priority, 
               thread->kernel_stack_base);
    
    return thread;
}

thread_t *thread_create(const char *name, thread_entry_t entry, void *arg) {
    return thread_create_priority(name, entry, arg, PRIORITY_NORMAL);
}

void thread_exit(void) {
    thread_t *t = thread_current();
    if (t) {
        DEBUG_INFO("Thread '%s' (TID=%d) exiting\n", t->name, t->tid);
        t->state = THREAD_STATE_TERMINATED;
        
        // Yield to let scheduler pick another thread
        // The scheduler should clean up terminated threads
        thread_yield();
    }
    
    // Should never get here
    while (1) {
        __asm__ volatile("hlt");
    }
}

void thread_yield(void) {
    extern void scheduler_yield(void);
    scheduler_yield();
}

void thread_sleep_ms(uint32_t ms) {
    extern void scheduler_sleep(thread_t *thread, uint64_t wake_time);
    extern uint64_t timer_get_ticks(void);
    
    thread_t *t = thread_current();
    if (t) {
        uint64_t wake_time = timer_get_ticks() + ms;  // At 1kHz, 1 tick = 1ms
        scheduler_sleep(t, wake_time);
    }
}

void thread_set_priority(thread_t *thread, uint8_t priority) {
    if (thread && priority < PRIORITY_LEVELS) {
        thread->priority = priority;
        thread->time_slice_length = calculate_time_slice(priority);
        DEBUG_INFO("Thread '%s' priority set to %d\n", thread->name, priority);
    }
}

// thread_current() is defined in scheduler.c
// We just provide thread_set_current() here

void thread_set_current(thread_t *thread) {
    // This is called by scheduler to update TSS - we just update TSS here
    if (thread) {
        uint64_t stack_top = thread->kernel_stack_base + thread->kernel_stack_size;
        gdt_set_kernel_stack(stack_top);
    }
}

uint32_t thread_get_tid(void) {
    thread_t *curr = thread_current();
    return curr ? curr->tid : 0;
}

thread_t *thread_get_by_id(uint32_t tid) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (all_threads[i] && all_threads[i]->tid == tid) {
            return all_threads[i];
        }
    }
    return NULL;
}

const char *thread_state_name(thread_state_t state) {
    switch (state) {
        case THREAD_STATE_CREATED:    return "CREATED";
        case THREAD_STATE_READY:      return "READY";
        case THREAD_STATE_RUNNING:    return "RUNNING";
        case THREAD_STATE_BLOCKED:    return "BLOCKED";
        case THREAD_STATE_SLEEPING:   return "SLEEPING";
        case THREAD_STATE_TERMINATED: return "TERMINATED";
        default:                      return "UNKNOWN";
    }
}

const char *thread_priority_name(uint8_t priority) {
    switch (priority) {
        case PRIORITY_REALTIME:       return "REALTIME";
        case PRIORITY_HIGH:           return "HIGH";
        case PRIORITY_ABOVE_NORMAL:   return "ABOVE_NORMAL";
        case PRIORITY_NORMAL:         return "NORMAL";
        case PRIORITY_BELOW_NORMAL:   return "BELOW_NORMAL";
        case PRIORITY_LOW:            return "LOW";
        case PRIORITY_IDLE:           return "IDLE";
        default:                      return "UNKNOWN";
    }
}

void thread_init(void) {
    DEBUG_INFO("Initializing thread subsystem\n");
    
    // Clear thread table
    memset(all_threads, 0, sizeof(all_threads));
    
    // The bootstrap "thread" (kernel main) will be set up by scheduler_init
    // current_thread is managed by scheduler.c
    
    DEBUG_INFO("Thread subsystem initialized\n");
}
