#include "timer.h"
#include "pic.h"
#include "../debug/debug.h"

// I/O port access functions
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Timer system state
static volatile uint64_t system_ticks = 0;
static uint32_t timer_frequency = DEFAULT_TIMER_FREQUENCY;
static timer_callback_t global_callback = NULL;
static timer_t *timer_list = NULL;

// Internal functions
static void timer_check_scheduled(void);
static void timer_add_to_list(timer_t *timer);
static void timer_remove_from_list(timer_t *timer);

bool timer_init(uint32_t frequency) {
    DEBUG_INFO("Initializing timer system with frequency %d Hz", frequency);
    
    timer_frequency = frequency;
    system_ticks = 0;
    
    // Calculate the PIT divisor
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 65535) {
        DEBUG_ERROR("Timer frequency too low, minimum is %d Hz", PIT_FREQUENCY / 65535);
        return false;
    }
    
    // Configure PIT channel 0 in mode 2 (rate generator)
    outb(PIT_COMMAND, PIT_CHANNEL0_SELECT | PIT_ACCESS_LOHI | PIT_MODE2 | PIT_BINARY);
    
    // Send the divisor
    outb(PIT_CHANNEL0, divisor & 0xFF);        // Low byte
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF); // High byte
    
    // Register the timer interrupt handler
    register_interrupt_handler(IRQ_TIMER, timer_interrupt_handler);
    
    // Enable timer IRQ
    pic_enable_irq(PIC_IRQ_TIMER);
    
    DEBUG_INFO("Timer initialized successfully with divisor %d", divisor);
    return true;
}

uint64_t timer_get_ticks(void) {
    return system_ticks;
}

uint64_t timer_get_uptime_ms(void) {
    return (system_ticks * 1000) / timer_frequency;
}

void timer_sleep_ms(uint32_t milliseconds) {
    uint64_t start_ticks = system_ticks;
    uint64_t target_ticks = start_ticks + (milliseconds * timer_frequency) / 1000;
    
    while (system_ticks < target_ticks) {
        // Simple busy wait with halt instruction
        asm volatile("hlt");
    }
}

void timer_sleep_ticks(uint32_t ticks) {
    uint64_t start_ticks = system_ticks;
    uint64_t target_ticks = start_ticks + ticks;
    
    while (system_ticks < target_ticks) {
        asm volatile("hlt");
    }
}

timer_t *timer_schedule(uint32_t delay_ms, timer_callback_t callback, void *data) {
    if (!callback) {
        return NULL;
    }
    
    // For now, we'll use a simple static allocation (in a real system, use dynamic allocation)
    static timer_t timers[16];  // Support up to 16 scheduled timers
    static int next_timer = 0;
    
    if (next_timer >= 16) {
        DEBUG_ERROR("Maximum number of scheduled timers exceeded");
        return NULL;
    }
    
    timer_t *timer = &timers[next_timer++];
    timer->expire_time = system_ticks + (delay_ms * timer_frequency) / 1000;
    timer->callback = callback;
    timer->data = data;
    timer->active = true;
    timer->next = NULL;
    
    timer_add_to_list(timer);
    
    DEBUG_INFO("Scheduled timer to expire in %d ms (tick %llu)", delay_ms, timer->expire_time);
    return timer;
}

bool timer_cancel(timer_t *timer) {
    if (!timer || !timer->active) {
        return false;
    }
    
    timer->active = false;
    timer_remove_from_list(timer);
    
    DEBUG_INFO("Cancelled timer");
    return true;
}

void timer_register_callback(timer_callback_t callback) {
    global_callback = callback;
}

void timer_unregister_callback(timer_callback_t callback) {
    if (global_callback == callback) {
        global_callback = NULL;
    }
}

uint32_t timer_get_frequency(void) {
    return timer_frequency;
}

bool timer_set_frequency(uint32_t frequency) {
    return timer_init(frequency);
}

void timer_interrupt_handler(interrupt_frame_t *frame) {
    (void)frame; // Suppress unused parameter warning
    
    // Increment system tick counter
    system_ticks++;
    
    // Add debug output for first few ticks
    if (system_ticks <= 5) {
        DEBUG_INFO("Timer interrupt tick: %lu", (unsigned long)system_ticks);
    }
    
    // Call global callback if registered
    if (global_callback) {
        global_callback(system_ticks);
    }
    
    // Check scheduled timers
    timer_check_scheduled();
    
    // Send EOI to PIC
    pic_send_eoi(PIC_IRQ_TIMER);
}

// Internal functions

static void timer_check_scheduled(void) {
    timer_t *current = timer_list;
    timer_t *prev = NULL;
    
    while (current) {
        if (current->active && system_ticks >= current->expire_time) {
            // Timer has expired, call the callback
            current->callback(system_ticks);
            
            // Remove from list
            if (prev) {
                prev->next = current->next;
            } else {
                timer_list = current->next;
            }
            
            current->active = false;
            current = prev ? prev->next : timer_list;
        } else {
            prev = current;
            current = current->next;
        }
    }
}

static void timer_add_to_list(timer_t *timer) {
    if (!timer_list) {
        timer_list = timer;
        return;
    }
    
    // Insert in order of expiration time
    if (timer->expire_time < timer_list->expire_time) {
        timer->next = timer_list;
        timer_list = timer;
        return;
    }
    
    timer_t *current = timer_list;
    while (current->next && current->next->expire_time < timer->expire_time) {
        current = current->next;
    }
    
    timer->next = current->next;
    current->next = timer;
}

static void timer_remove_from_list(timer_t *timer) {
    if (!timer_list) {
        return;
    }
    
    if (timer_list == timer) {
        timer_list = timer->next;
        return;
    }
    
    timer_t *current = timer_list;
    while (current->next && current->next != timer) {
        current = current->next;
    }
    
    if (current->next == timer) {
        current->next = timer->next;
    }
}