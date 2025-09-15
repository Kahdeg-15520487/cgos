#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "idt.h"

// PIT (Programmable Interval Timer) constants
#define PIT_FREQUENCY       1193182     // PIT base frequency in Hz
#define PIT_CHANNEL0        0x40        // Channel 0 data port
#define PIT_CHANNEL1        0x41        // Channel 1 data port
#define PIT_CHANNEL2        0x42        // Channel 2 data port
#define PIT_COMMAND         0x43        // Mode/Command register

// PIT command register bits
#define PIT_BINARY          0x00        // Binary mode
#define PIT_BCD             0x01        // BCD mode
#define PIT_MODE0           0x00        // Mode 0: Interrupt on terminal count
#define PIT_MODE1           0x02        // Mode 1: Hardware re-triggerable one-shot
#define PIT_MODE2           0x04        // Mode 2: Rate generator
#define PIT_MODE3           0x06        // Mode 3: Square wave generator
#define PIT_MODE4           0x08        // Mode 4: Software triggered strobe
#define PIT_MODE5           0x0A        // Mode 5: Hardware triggered strobe
#define PIT_ACCESS_LATCH    0x00        // Latch count value command
#define PIT_ACCESS_LO       0x10        // Access low byte only
#define PIT_ACCESS_HI       0x20        // Access high byte only
#define PIT_ACCESS_LOHI     0x30        // Access low byte then high byte
#define PIT_CHANNEL0_SELECT 0x00        // Select channel 0
#define PIT_CHANNEL1_SELECT 0x40        // Select channel 1
#define PIT_CHANNEL2_SELECT 0x80        // Select channel 2

// Default timer frequency (100 Hz = 10ms intervals)
#define DEFAULT_TIMER_FREQUENCY 100

// Timer callback function type
typedef void (*timer_callback_t)(uint64_t ticks);

// Timer structure for managing scheduled callbacks
typedef struct timer {
    uint64_t expire_time;       // When this timer should expire
    timer_callback_t callback;  // Function to call when timer expires
    void *data;                 // User data to pass to callback
    bool active;                // Whether this timer is active
    struct timer *next;         // Next timer in the list
} timer_t;

// Timer system functions

// Initialize the timer system
bool timer_init(uint32_t frequency);

// Get the current tick count
uint64_t timer_get_ticks(void);

// Get the current time in milliseconds since boot
uint64_t timer_get_uptime_ms(void);

// Sleep for a specified number of milliseconds
void timer_sleep_ms(uint32_t milliseconds);

// Sleep for a specified number of ticks
void timer_sleep_ticks(uint32_t ticks);

// Schedule a callback to be called after a delay
timer_t *timer_schedule(uint32_t delay_ms, timer_callback_t callback, void *data);

// Cancel a scheduled timer
bool timer_cancel(timer_t *timer);

// Register a timer callback (called every tick)
void timer_register_callback(timer_callback_t callback);

// Unregister a timer callback
void timer_unregister_callback(timer_callback_t callback);

// Get timer frequency
uint32_t timer_get_frequency(void);

// Set timer frequency
bool timer_set_frequency(uint32_t frequency);

// Internal timer interrupt handler
void timer_interrupt_handler(interrupt_frame_t *frame);

#endif // TIMER_H