#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <stdbool.h>

// Thread states
typedef enum {
    THREAD_STATE_CREATED,       // Just created, not yet scheduled
    THREAD_STATE_READY,         // Ready to run
    THREAD_STATE_RUNNING,       // Currently executing
    THREAD_STATE_BLOCKED,       // Waiting for something (I/O, lock, etc.)
    THREAD_STATE_SLEEPING,      // Sleeping for a specified time
    THREAD_STATE_TERMINATED     // Finished execution
} thread_state_t;

// Priority levels (0 = highest, 6 = lowest/idle)
#define PRIORITY_REALTIME       0
#define PRIORITY_HIGH           1
#define PRIORITY_ABOVE_NORMAL   2
#define PRIORITY_NORMAL         3   // Default for new threads
#define PRIORITY_BELOW_NORMAL   4
#define PRIORITY_LOW            5
#define PRIORITY_IDLE           6
#define PRIORITY_LEVELS         7

// Scheduling parameters
#define TIME_SLICE_BASE_MS      10      // 10ms base time slice
#define KERNEL_STACK_SIZE       8192    // 8KB kernel stack per thread
#define CPU_HISTORY_SAMPLES     8       // Samples for CPU usage tracking
#define PRIORITY_BOOST_THRESHOLD    30  // Boost if CPU usage < 30%
#define PRIORITY_DEMOTE_THRESHOLD   80  // Demote if CPU usage > 80%
#define MAX_THREADS             256     // Maximum concurrent threads

// Forward declaration
struct thread;
typedef struct thread thread_t;

// Thread entry function type
typedef void (*thread_entry_t)(void *arg);

// Thread Control Block (TCB)
struct thread {
    // ===== Must be at fixed offsets for assembly access =====
    uint32_t tid;                   // Offset 0: Thread ID
    thread_state_t state;           // Offset 4: Current state
    
    // Stack pointers (for context switch)
    uint64_t kernel_stack_base;     // Offset 8: Bottom of kernel stack (for freeing)
    uint64_t kernel_stack_size;     // Offset 16: Size of kernel stack
    uint64_t rsp;                   // Offset 24: Saved stack pointer (for context switch)
    // ==========================================================
    
    char name[32];                  // Thread name for debugging
    
    // Entry point info
    thread_entry_t entry;           // Entry function
    void *arg;                      // Argument to entry function
    
    // Scheduling fields
    uint8_t priority;               // Current priority (may differ from base)
    uint8_t base_priority;          // Original assigned priority
    uint32_t time_slice;            // Remaining ticks in current time slice
    uint32_t time_slice_length;     // Full time slice length for this priority
    uint64_t total_ticks;           // Total CPU ticks consumed (lifetime)
    
    // Adaptive scheduling - CPU usage tracking
    uint8_t cpu_usage_history[CPU_HISTORY_SAMPLES];  // Recent CPU usage percentages
    uint8_t history_index;          // Current index in history ring buffer
    uint8_t avg_cpu_usage;          // Smoothed average CPU usage
    uint64_t slice_start_ticks;     // Timer tick when current slice started
    uint64_t ticks_used_this_slice; // Ticks used in current slice
    
    // Sleep support
    uint64_t wake_time;             // Timer tick at which to wake up
    
    // Queue linkage (for ready/sleep/blocked queues)
    thread_t *next;                 // Next thread in queue
    thread_t *prev;                 // Previous thread in queue
    
    // Exit code
    int exit_code;
};

// ============== Thread API ==============

// Create a new kernel thread with default priority (PRIORITY_NORMAL)
thread_t *thread_create(const char *name, thread_entry_t entry, void *arg);

// Create a new kernel thread with specified priority
thread_t *thread_create_priority(const char *name, thread_entry_t entry, 
                                  void *arg, uint8_t priority);

// Exit the current thread
void thread_exit(void) __attribute__((noreturn));

// Voluntarily yield the CPU to another thread
void thread_yield(void);

// Sleep for specified milliseconds
void thread_sleep_ms(uint32_t ms);

// Set a thread's priority
void thread_set_priority(thread_t *thread, uint8_t priority);

// Get the currently running thread
thread_t *thread_current(void);

// Get current thread's ID
uint32_t thread_get_tid(void);

// Get thread by ID (NULL if not found)
thread_t *thread_get_by_id(uint32_t tid);

// Get thread state as string (for debugging)
const char *thread_state_name(thread_state_t state);

// Get priority name as string (for debugging)  
const char *thread_priority_name(uint8_t priority);

// Initialize thread subsystem (called once at boot)
void thread_init(void);

#endif // THREAD_H
