#include "sync.h"
#include "idt.h"
#include "timer.h"
#include "../debug/debug.h"

// Debug statistics
static volatile uint64_t spinlock_acquisitions = 0;
static volatile uint64_t spinlock_contentions = 0;

// Spinlock functions

void spinlock_init(spinlock_t *lock, const char *name) {
    if (!lock) return;
    
    atomic_flag_clear(&lock->locked);
    lock->name = name ? name : "unnamed";
    lock->lock_time = 0;
}

void spinlock_acquire(spinlock_t *lock) {
    if (!lock) return;
    
    // Busy wait until we can acquire the lock
    while (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        // Use pause instruction to be friendly to hyperthreading
        cpu_pause();
        atomic_inc(&spinlock_contentions);
    }
    
    lock->lock_time = timer_get_ticks();
    atomic_inc(&spinlock_acquisitions);
}

bool spinlock_try_acquire(spinlock_t *lock) {
    if (!lock) return false;
    
    if (!atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        lock->lock_time = timer_get_ticks();
        atomic_inc(&spinlock_acquisitions);
        return true;
    }
    
    return false;
}

void spinlock_release(spinlock_t *lock) {
    if (!lock) return;
    
    lock->lock_time = 0;
    atomic_flag_clear_explicit(&lock->locked, memory_order_release);
}

bool spinlock_is_locked(spinlock_t *lock) {
    if (!lock) return false;
    
    return atomic_flag_test_and_set_explicit(&lock->locked, memory_order_relaxed);
}

void spinlock_acquire_irq_save(spinlock_t *lock, irq_state_t *state) {
    if (!lock || !state) return;
    
    // Save current interrupt state and disable interrupts
    state->interrupts_were_enabled = interrupts_enabled();
    disable_interrupts();
    
    // Now acquire the spinlock
    spinlock_acquire(lock);
}

void spinlock_release_irq_restore(spinlock_t *lock, irq_state_t *state) {
    if (!lock || !state) return;
    
    // Release the spinlock first
    spinlock_release(lock);
    
    // Restore interrupt state
    if (state->interrupts_were_enabled) {
        enable_interrupts();
    }
}

// Mutex functions (simplified implementation without blocking)

void mutex_init(mutex_t *mutex, const char *name) {
    if (!mutex) return;
    
    spinlock_init(&mutex->lock, name);
    mutex->locked = false;
    mutex->name = name ? name : "unnamed_mutex";
}

void mutex_acquire(mutex_t *mutex) {
    if (!mutex) return;
    
    // Simple busy-wait implementation (in a full OS, this would block the thread)
    while (true) {
        spinlock_acquire(&mutex->lock);
        
        if (!mutex->locked) {
            mutex->locked = true;
            spinlock_release(&mutex->lock);
            break;
        }
        
        spinlock_release(&mutex->lock);
        cpu_pause();
    }
}

bool mutex_try_acquire(mutex_t *mutex) {
    if (!mutex) return false;
    
    spinlock_acquire(&mutex->lock);
    
    if (!mutex->locked) {
        mutex->locked = true;
        spinlock_release(&mutex->lock);
        return true;
    }
    
    spinlock_release(&mutex->lock);
    return false;
}

void mutex_release(mutex_t *mutex) {
    if (!mutex) return;
    
    spinlock_acquire(&mutex->lock);
    mutex->locked = false;
    spinlock_release(&mutex->lock);
}

// Semaphore functions

void semaphore_init(semaphore_t *sem, int initial_count, int max_count, const char *name) {
    if (!sem) return;
    
    spinlock_init(&sem->lock, name);
    sem->count = initial_count;
    sem->max_count = max_count;
    sem->name = name ? name : "unnamed_semaphore";
}

void semaphore_wait(semaphore_t *sem) {
    if (!sem) return;
    
    // Busy wait for a semaphore slot
    while (true) {
        spinlock_acquire(&sem->lock);
        
        if (sem->count > 0) {
            sem->count--;
            spinlock_release(&sem->lock);
            break;
        }
        
        spinlock_release(&sem->lock);
        cpu_pause();
    }
}

bool semaphore_try_wait(semaphore_t *sem) {
    if (!sem) return false;
    
    spinlock_acquire(&sem->lock);
    
    if (sem->count > 0) {
        sem->count--;
        spinlock_release(&sem->lock);
        return true;
    }
    
    spinlock_release(&sem->lock);
    return false;
}

void semaphore_signal(semaphore_t *sem) {
    if (!sem) return;
    
    spinlock_acquire(&sem->lock);
    
    if (sem->count < sem->max_count) {
        sem->count++;
    }
    
    spinlock_release(&sem->lock);
}

int semaphore_get_count(semaphore_t *sem) {
    if (!sem) return -1;
    
    spinlock_acquire(&sem->lock);
    int count = sem->count;
    spinlock_release(&sem->lock);
    
    return count;
}

// Critical section functions

irq_state_t critical_section_enter(void) {
    irq_state_t state;
    state.interrupts_were_enabled = interrupts_enabled();
    disable_interrupts();
    return state;
}

void critical_section_exit(irq_state_t state) {
    if (state.interrupts_were_enabled) {
        enable_interrupts();
    }
}

// Debug function to get synchronization statistics
void sync_print_stats(void) {
    DEBUG_INFO("Synchronization Statistics:");
    DEBUG_INFO("  Spinlock acquisitions: %llu", spinlock_acquisitions);
    DEBUG_INFO("  Spinlock contentions: %llu", spinlock_contentions);
    if (spinlock_acquisitions > 0) {
        DEBUG_INFO("  Contention rate: %llu%%", 
                  (spinlock_contentions * 100) / spinlock_acquisitions);
    }
}