#include "scheduler.h"
#include "context.h"
#include "../debug/debug.h"
#include "../timer/timer.h"
#include "../gdt/gdt.h"
#include <string.h>

// ============== Data Structures ==============

// Per-priority ready queues (circular doubly-linked lists)
static thread_t *ready_queue_heads[PRIORITY_LEVELS];
static thread_t *ready_queue_tails[PRIORITY_LEVELS];

// Sleep queue (ordered by wake_time, singly linked)
static thread_t *sleep_queue;

// Blocked queue (singly linked)
static thread_t *blocked_queue;

// Idle thread - runs when no other thread is ready
static thread_t *idle_thread;

// Current running thread
static thread_t *current_thread = NULL;

// Scheduler statistics
static scheduler_stats_t stats;

// Is scheduler running?
static bool scheduler_running = false;

// Scheduling lock (simple spinlock for now - interrupts disabled)
static volatile bool sched_lock = false;

// ============== Queue Operations ==============

// Add thread to tail of a priority queue
static void enqueue_ready(thread_t *thread) {
    uint8_t p = thread->priority;
    thread->next = NULL;
    thread->prev = ready_queue_tails[p];
    
    if (ready_queue_tails[p]) {
        ready_queue_tails[p]->next = thread;
    } else {
        ready_queue_heads[p] = thread;
    }
    ready_queue_tails[p] = thread;
    
    thread->state = THREAD_STATE_READY;
    stats.threads_ready++;
}

// Remove thread from head of a priority queue
static thread_t *dequeue_ready(uint8_t priority) {
    thread_t *thread = ready_queue_heads[priority];
    if (!thread) return NULL;
    
    ready_queue_heads[priority] = thread->next;
    if (ready_queue_heads[priority]) {
        ready_queue_heads[priority]->prev = NULL;
    } else {
        ready_queue_tails[priority] = NULL;
    }
    
    thread->next = NULL;
    thread->prev = NULL;
    stats.threads_ready--;
    
    return thread;
}

// Remove specific thread from its ready queue
static void remove_from_ready(thread_t *thread) {
    uint8_t p = thread->priority;
    
    if (thread->prev) {
        thread->prev->next = thread->next;
    } else {
        ready_queue_heads[p] = thread->next;
    }
    
    if (thread->next) {
        thread->next->prev = thread->prev;
    } else {
        ready_queue_tails[p] = thread->prev;
    }
    
    thread->next = NULL;
    thread->prev = NULL;
    stats.threads_ready--;
}

// Add thread to sleep queue (ordered by wake_time)
static void enqueue_sleep(thread_t *thread) {
    thread->state = THREAD_STATE_SLEEPING;
    thread->next = NULL;
    
    // Insert in sorted order
    if (!sleep_queue || thread->wake_time < sleep_queue->wake_time) {
        thread->next = sleep_queue;
        sleep_queue = thread;
    } else {
        thread_t *prev = sleep_queue;
        while (prev->next && prev->next->wake_time <= thread->wake_time) {
            prev = prev->next;
        }
        thread->next = prev->next;
        prev->next = thread;
    }
    
    stats.threads_sleeping++;
}

// ============== Adaptive Priority ==============

// Update CPU usage history for a thread
static void update_cpu_usage(thread_t *thread, uint8_t usage_percent) {
    thread->cpu_usage_history[thread->history_index] = usage_percent;
    thread->history_index = (thread->history_index + 1) % CPU_HISTORY_SAMPLES;
    
    // Calculate moving average
    uint16_t sum = 0;
    for (int i = 0; i < CPU_HISTORY_SAMPLES; i++) {
        sum += thread->cpu_usage_history[i];
    }
    thread->avg_cpu_usage = sum / CPU_HISTORY_SAMPLES;
}

// Adjust priority based on CPU usage pattern
static void adjust_priority(thread_t *thread) {
    // Don't adjust idle thread or realtime threads
    if (thread == idle_thread || thread->base_priority == PRIORITY_REALTIME) {
        return;
    }
    
    uint8_t old_priority = thread->priority;
    
    // CPU-bound (uses lots of CPU) -> demote priority
    if (thread->avg_cpu_usage > PRIORITY_DEMOTE_THRESHOLD) {
        if (thread->priority < PRIORITY_LOW) {
            thread->priority++;
            stats.priority_demotions++;
            DEBUG_INFO("Thread '%s' demoted %d->%d (CPU: %d%%)\n",
                       thread->name, old_priority, thread->priority,
                       thread->avg_cpu_usage);
        }
    }
    // I/O-bound (yields early, low CPU) -> boost priority
    else if (thread->avg_cpu_usage < PRIORITY_BOOST_THRESHOLD) {
        if (thread->priority > thread->base_priority) {
            thread->priority--;
            stats.priority_boosts++;
            DEBUG_INFO("Thread '%s' boosted %d->%d (CPU: %d%%)\n",
                       thread->name, old_priority, thread->priority,
                       thread->avg_cpu_usage);
        }
    }
    
    // Update time slice if priority changed
    if (thread->priority != old_priority) {
        thread->time_slice_length = TIME_SLICE_BASE_MS + 
                                     (PRIORITY_LEVELS - thread->priority) * 3;
    }
}

// ============== Core Scheduler ==============

// Pick the next thread to run
static thread_t *pick_next_thread(void) {
    // Scan priority queues from highest (0) to lowest (PRIORITY_LEVELS-1)
    for (int p = 0; p < PRIORITY_LEVELS; p++) {
        if (ready_queue_heads[p]) {
            return dequeue_ready(p);
        }
    }
    
    // Nothing ready - return idle thread
    return idle_thread;
}

// Wake threads whose sleep time has elapsed
static void wake_expired_sleepers(void) {
    uint64_t now = timer_get_ticks();
    
    while (sleep_queue && sleep_queue->wake_time <= now) {
        thread_t *thread = sleep_queue;
        sleep_queue = sleep_queue->next;
        thread->next = NULL;
        stats.threads_sleeping--;
        
        DEBUG_INFO("Waking thread '%s' (TID=%d)\n", thread->name, thread->tid);
        enqueue_ready(thread);
    }
}

// Perform the actual context switch
static void switch_to(thread_t *next) {
    if (next == current_thread) {
        return;  // Already running this thread
    }
    
    thread_t *prev = current_thread;
    current_thread = next;
    next->state = THREAD_STATE_RUNNING;
    
    // Update TSS with new thread's kernel stack
    uint64_t stack_top = next->kernel_stack_base + next->kernel_stack_size;
    gdt_set_kernel_stack(stack_top);
    
    // Reset time slice for new thread
    next->time_slice = next->time_slice_length;
    next->slice_start_ticks = timer_get_ticks();
    next->ticks_used_this_slice = 0;
    
    stats.total_switches++;
    
    // Actually switch
    context_switch(prev, next);
}

// Internal: schedule the next thread
static void schedule(void) {
    thread_t *next = pick_next_thread();
    
    if (next != current_thread) {
        switch_to(next);
    }
}

// ============== Public API ==============

// Idle thread function - runs when no other thread is ready
static void idle_thread_entry(void *arg) {
    (void)arg;
    while (1) {
        // Enable interrupts and halt until next interrupt
        __asm__ volatile("sti; hlt");
    }
}

void scheduler_init(void) {
    DEBUG_INFO("Initializing scheduler...\n");
    
    // Clear all queues
    memset(ready_queue_heads, 0, sizeof(ready_queue_heads));
    memset(ready_queue_tails, 0, sizeof(ready_queue_tails));
    sleep_queue = NULL;
    blocked_queue = NULL;
    
    // Clear statistics
    memset(&stats, 0, sizeof(stats));
    
    // Initialize thread subsystem
    thread_init();
    
    // Create idle thread
    idle_thread = thread_create_priority("idle", idle_thread_entry, NULL, PRIORITY_IDLE);
    if (!idle_thread) {
        DEBUG_ERROR("Failed to create idle thread!\n");
        return;
    }
    
    // Idle thread doesn't go in ready queue - it's the fallback
    idle_thread->state = THREAD_STATE_READY;
    
    // Current "thread" is the bootstrap kernel
    // We'll create a proper thread for it when scheduler_start is called
    
    DEBUG_INFO("Scheduler initialized\n");
}

void scheduler_add(thread_t *thread) {
    if (!thread) return;
    
    // Disable interrupts for queue manipulation
    __asm__ volatile("cli");
    
    enqueue_ready(thread);
    
    __asm__ volatile("sti");
}

void scheduler_remove(thread_t *thread) {
    if (!thread) return;
    
    __asm__ volatile("cli");
    
    if (thread->state == THREAD_STATE_READY) {
        remove_from_ready(thread);
    }
    // TODO: remove from sleep queue if sleeping
    // TODO: remove from blocked queue if blocked
    
    __asm__ volatile("sti");
}

void scheduler_tick(void) {
    if (!scheduler_running || !current_thread) return;
    
    // Track idle time
    if (current_thread == idle_thread) {
        stats.idle_ticks++;
    }
    
    // Wake sleeping threads
    wake_expired_sleepers();
    
    // Track CPU usage for current thread
    current_thread->total_ticks++;
    current_thread->ticks_used_this_slice++;
    
    // Decrement time slice
    if (current_thread->time_slice > 0) {
        current_thread->time_slice--;
    }
    
    // Time slice expired?
    if (current_thread->time_slice == 0 && current_thread != idle_thread) {
        // Calculate CPU usage for this slice
        // 100% means used the entire slice
        uint64_t slice_len = current_thread->time_slice_length;
        uint8_t usage = (current_thread->ticks_used_this_slice * 100) / slice_len;
        if (usage > 100) usage = 100;
        
        update_cpu_usage(current_thread, usage);
        adjust_priority(current_thread);
        
        // Put current thread back in ready queue
        enqueue_ready(current_thread);
        
        // Pick next thread
        schedule();
    }
}

void scheduler_yield(void) {
    __asm__ volatile("cli");
    
    if (!scheduler_running || !current_thread) {
        __asm__ volatile("sti");
        return;
    }
    
    // Calculate partial CPU usage (yielded early = low usage)
    if (current_thread != idle_thread && current_thread->time_slice_length > 0) {
        uint8_t usage = (current_thread->ticks_used_this_slice * 100) / 
                        current_thread->time_slice_length;
        update_cpu_usage(current_thread, usage);
        adjust_priority(current_thread);
    }
    
    // Current thread goes back to ready queue (unless terminated)
    if (current_thread->state != THREAD_STATE_TERMINATED) {
        enqueue_ready(current_thread);
    }
    
    // Pick next thread (interrupts re-enabled by context switch)
    schedule();
    
    // We return here when this thread is scheduled again
    __asm__ volatile("sti");
}

void scheduler_block(thread_t *thread) {
    __asm__ volatile("cli");
    
    thread->state = THREAD_STATE_BLOCKED;
    stats.threads_blocked++;
    
    // If blocking current thread, need to switch
    if (thread == current_thread) {
        schedule();
    }
    
    __asm__ volatile("sti");
}

void scheduler_unblock(thread_t *thread) {
    __asm__ volatile("cli");
    
    if (thread->state == THREAD_STATE_BLOCKED) {
        stats.threads_blocked--;
        enqueue_ready(thread);
    }
    
    __asm__ volatile("sti");
}

void scheduler_sleep(thread_t *thread, uint64_t wake_time) {
    __asm__ volatile("cli");
    
    thread->wake_time = wake_time;
    enqueue_sleep(thread);
    
    // If sleeping current thread, need to switch
    if (thread == current_thread) {
        schedule();
    }
    
    __asm__ volatile("sti");
}

void scheduler_start(void) {
    DEBUG_INFO("Starting scheduler...\n");
    
    __asm__ volatile("cli");
    
    scheduler_running = true;
    
    // Pick first thread to run
    thread_t *first = pick_next_thread();
    if (!first) {
        first = idle_thread;
    }
    
    first->state = THREAD_STATE_RUNNING;
    current_thread = first;
    first->slice_start_ticks = timer_get_ticks();
    
    // Update TSS
    uint64_t stack_top = first->kernel_stack_base + first->kernel_stack_size;
    gdt_set_kernel_stack(stack_top);
    
    DEBUG_INFO("First thread: '%s' (TID=%d)\n", first->name, first->tid);
    
    // Jump to first thread - this is a one-way switch
    // We set up a fake "previous" thread that we don't care about saving
    static thread_t bootstrap_thread;
    memset(&bootstrap_thread, 0, sizeof(bootstrap_thread));
    bootstrap_thread.rsp = 0;  // Won't be used
    
    // This will "return" to the first thread's entry point
    context_switch(&bootstrap_thread, first);
    
    // Should never reach here
    while (1) {
        __asm__ volatile("hlt");
    }
}

bool scheduler_is_running(void) {
    return scheduler_running;
}

void scheduler_get_stats(scheduler_stats_t *out_stats) {
    if (out_stats) {
        *out_stats = stats;
    }
}

void scheduler_print_threads(void) {
    DEBUG_INFO("=== Thread List ===\n");
    DEBUG_INFO("Current: %s (TID=%d)\n", 
               current_thread ? current_thread->name : "none",
               current_thread ? current_thread->tid : 0);
    
    DEBUG_INFO("Ready queues:\n");
    for (int p = 0; p < PRIORITY_LEVELS; p++) {
        if (ready_queue_heads[p]) {
            DEBUG_INFO("  Priority %d:\n", p);
            thread_t *t = ready_queue_heads[p];
            while (t) {
                DEBUG_INFO("    - %s (TID=%d, CPU=%d%%)\n", 
                          t->name, t->tid, t->avg_cpu_usage);
                t = t->next;
            }
        }
    }
    
    if (sleep_queue) {
        DEBUG_INFO("Sleep queue:\n");
        thread_t *t = sleep_queue;
        while (t) {
            DEBUG_INFO("  - %s (TID=%d, wake@%lu)\n", 
                      t->name, t->tid, t->wake_time);
            t = t->next;
        }
    }
    
    DEBUG_INFO("Stats: switches=%u, boosts=%u, demotes=%u\n",
               stats.total_switches, stats.priority_boosts, 
               stats.priority_demotions);
}

// Get the currently running thread
thread_t *thread_current(void) {
    return current_thread;
}
