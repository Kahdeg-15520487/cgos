#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

// Spinlock structure
typedef struct {
    volatile atomic_flag locked;
    const char *name;       // For debugging
    uint64_t lock_time;     // When the lock was acquired (for debugging)
} spinlock_t;

// Mutex structure (future implementation)
typedef struct {
    spinlock_t lock;
    volatile bool locked;
    const char *name;
} mutex_t;

// Semaphore structure (basic implementation)
typedef struct {
    spinlock_t lock;
    volatile int count;
    int max_count;
    const char *name;
} semaphore_t;

// Interrupt state for critical sections
typedef struct {
    bool interrupts_were_enabled;
} irq_state_t;

// Spinlock functions

// Initialize a spinlock
void spinlock_init(spinlock_t *lock, const char *name);

// Acquire a spinlock (busy wait)
void spinlock_acquire(spinlock_t *lock);

// Try to acquire a spinlock without blocking
bool spinlock_try_acquire(spinlock_t *lock);

// Release a spinlock
void spinlock_release(spinlock_t *lock);

// Check if a spinlock is currently locked
bool spinlock_is_locked(spinlock_t *lock);

// Spinlock with interrupt disable (for interrupt handler synchronization)
void spinlock_acquire_irq_save(spinlock_t *lock, irq_state_t *state);
void spinlock_release_irq_restore(spinlock_t *lock, irq_state_t *state);

// Mutex functions (simplified, no blocking/scheduling yet)

// Initialize a mutex
void mutex_init(mutex_t *mutex, const char *name);

// Acquire a mutex
void mutex_acquire(mutex_t *mutex);

// Try to acquire a mutex without blocking
bool mutex_try_acquire(mutex_t *mutex);

// Release a mutex
void mutex_release(mutex_t *mutex);

// Semaphore functions

// Initialize a semaphore
void semaphore_init(semaphore_t *sem, int initial_count, int max_count, const char *name);

// Wait on a semaphore (decrement)
void semaphore_wait(semaphore_t *sem);

// Try to wait on a semaphore without blocking
bool semaphore_try_wait(semaphore_t *sem);

// Signal a semaphore (increment)
void semaphore_signal(semaphore_t *sem);

// Get the current semaphore count
int semaphore_get_count(semaphore_t *sem);

// Critical section functions

// Enter critical section (disable interrupts)
irq_state_t critical_section_enter(void);

// Exit critical section (restore interrupt state)
void critical_section_exit(irq_state_t state);

// Memory barrier functions

// Full memory barrier
static inline void memory_barrier(void) {
    atomic_thread_fence(memory_order_seq_cst);
}

// Read memory barrier
static inline void read_barrier(void) {
    atomic_thread_fence(memory_order_acquire);
}

// Write memory barrier
static inline void write_barrier(void) {
    atomic_thread_fence(memory_order_release);
}

// CPU pause instruction (for busy waiting)
static inline void cpu_pause(void) {
    asm volatile("pause" ::: "memory");
}

// Atomic operations wrappers

// Atomic compare and swap
static inline bool atomic_cmpxchg(volatile uint64_t *ptr, uint64_t expected, uint64_t desired) {
    return atomic_compare_exchange_strong((atomic_uint_fast64_t*)ptr, &expected, desired);
}

// Atomic increment
static inline uint64_t atomic_inc(volatile uint64_t *ptr) {
    return atomic_fetch_add((atomic_uint_fast64_t*)ptr, 1);
}

// Atomic decrement
static inline uint64_t atomic_dec(volatile uint64_t *ptr) {
    return atomic_fetch_sub((atomic_uint_fast64_t*)ptr, 1);
}

#endif // SYNC_H