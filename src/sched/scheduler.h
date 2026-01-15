#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "thread.h"

// ============== Scheduler Statistics ==============
typedef struct {
    uint32_t total_switches;        // Total context switches performed
    uint32_t threads_ready;         // Current number of ready threads
    uint32_t threads_sleeping;      // Current number of sleeping threads
    uint32_t threads_blocked;       // Current number of blocked threads
    uint32_t priority_boosts;       // Number of priority boosts (I/O-bound detection)
    uint32_t priority_demotions;    // Number of priority demotions (CPU-bound detection)
    uint64_t idle_ticks;            // Ticks spent in idle thread
} scheduler_stats_t;

// ============== Scheduler API ==============

// Initialize the scheduler subsystem
// Creates the idle thread and sets up data structures
void scheduler_init(void);

// Add a thread to the ready queue
// Thread must be in CREATED or READY state
void scheduler_add(thread_t *thread);

// Remove a thread from all scheduler queues
void scheduler_remove(thread_t *thread);

// Called from timer interrupt every tick
// Handles preemption, wakes sleeping threads, adjusts priorities
void scheduler_tick(void);

// Voluntarily yield CPU to another ready thread
// Current thread goes back to ready queue
void scheduler_yield(void);

// Block the current thread
// Thread is removed from ready queue until unblocked
void scheduler_block(thread_t *thread);

// Unblock a thread (e.g., I/O completed, lock acquired)
// Thread is added back to ready queue
void scheduler_unblock(thread_t *thread);

// Put a thread to sleep until wake_time (in timer ticks)
void scheduler_sleep(thread_t *thread, uint64_t wake_time);

// Start the scheduler
// Picks the first ready thread and switches to it
// This function never returns
void scheduler_start(void) __attribute__((noreturn));

// Check if scheduler is running
bool scheduler_is_running(void);

// Get scheduler statistics
void scheduler_get_stats(scheduler_stats_t *stats);

// Debug: Print all threads and their states
void scheduler_print_threads(void);

#endif // SCHEDULER_H
