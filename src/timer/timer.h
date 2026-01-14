#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "../pci/pci.h"  // For outb, inb

// PIT (Programmable Interval Timer) ports
#define PIT_CHANNEL0_DATA   0x40
#define PIT_CHANNEL1_DATA   0x41
#define PIT_CHANNEL2_DATA   0x42
#define PIT_COMMAND         0x43

// PIT command byte fields
#define PIT_CMD_CHANNEL0    0x00
#define PIT_CMD_ACCESS_LO   0x10
#define PIT_CMD_ACCESS_HI   0x20
#define PIT_CMD_ACCESS_LOHI 0x30
#define PIT_CMD_MODE_SQUARE 0x06  // Mode 3: Square wave generator
#define PIT_CMD_BINARY      0x00

// PIC (8259) ports
#define PIC1_COMMAND        0x20
#define PIC1_DATA           0x21
#define PIC2_COMMAND        0xA0
#define PIC2_DATA           0xA1

// PIC commands
#define PIC_EOI             0x20  // End of interrupt

// ICW1 (Initialization Command Word 1)
#define ICW1_ICW4           0x01  // ICW4 needed
#define ICW1_INIT           0x10  // Initialization

// ICW4
#define ICW4_8086           0x01  // 8086/88 mode

// Timer frequency
#define PIT_FREQUENCY       1193182  // ~1.193182 MHz
#define TIMER_FREQUENCY_HZ  1000     // 1000 Hz = 1ms per tick

// IRQ numbers
#define IRQ_TIMER           0
#define IRQ_KEYBOARD        1

// IDT vector for timer (remapped PIC)
#define TIMER_VECTOR        32

// Function declarations
void timer_init(void);
uint64_t timer_get_ticks(void);
uint32_t timer_get_seconds(void);
void timer_sleep_ms(uint32_t ms);
void timer_irq_handler(void);

// PIC functions
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

// io_wait - small delay using unused port
static inline void io_wait(void) {
    outb(0x80, 0);  // Write to unused port for small delay
}

#endif // TIMER_H
