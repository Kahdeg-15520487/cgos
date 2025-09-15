#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include <stdbool.h>

// PIC I/O port definitions
#define PIC1_COMMAND    0x20        // Master PIC command port
#define PIC1_DATA       0x21        // Master PIC data port
#define PIC2_COMMAND    0xA0        // Slave PIC command port
#define PIC2_DATA       0xA1        // Slave PIC data port

// PIC command definitions
#define PIC_EOI         0x20        // End of Interrupt command

// ICW1 (Initialization Command Word 1)
#define ICW1_ICW4       0x01        // ICW4 needed
#define ICW1_SINGLE     0x02        // Single (cascade) mode
#define ICW1_INTERVAL4  0x04        // Call address interval 4 (8)
#define ICW1_LEVEL      0x08        // Level triggered (edge) mode
#define ICW1_INIT       0x10        // Initialization required

// ICW4 (Initialization Command Word 4)
#define ICW4_8086       0x01        // 8086/88 (MCS-80/85) mode
#define ICW4_AUTO       0x02        // Auto (normal) EOI
#define ICW4_BUF_SLAVE  0x08        // Buffered mode/slave
#define ICW4_BUF_MASTER 0x0C        // Buffered mode/master
#define ICW4_SFNM       0x10        // Special fully nested mode

// Default IRQ mappings
#define PIC1_OFFSET     0x20        // Master PIC interrupt offset
#define PIC2_OFFSET     0x28        // Slave PIC interrupt offset

// IRQ numbers (hardware IRQ numbers, not interrupt vector numbers)
#define PIC_IRQ_TIMER       0
#define PIC_IRQ_KEYBOARD    1
#define PIC_IRQ_CASCADE     2
#define PIC_IRQ_COM2        3
#define PIC_IRQ_COM1        4
#define PIC_IRQ_LPT2        5
#define PIC_IRQ_FLOPPY      6
#define PIC_IRQ_LPT1        7
#define PIC_IRQ_RTC         8
#define PIC_IRQ_FREE1       9
#define PIC_IRQ_FREE2       10
#define PIC_IRQ_FREE3       11
#define PIC_IRQ_MOUSE       12
#define PIC_IRQ_FPU         13
#define PIC_IRQ_ATA1        14
#define PIC_IRQ_ATA2        15

// PIC management functions

// Initialize the PIC with specified interrupt offsets
bool pic_init(uint8_t master_offset, uint8_t slave_offset);

// Send End of Interrupt to appropriate PIC
void pic_send_eoi(uint8_t irq);

// Enable a specific IRQ
void pic_enable_irq(uint8_t irq);

// Disable a specific IRQ
void pic_disable_irq(uint8_t irq);

// Enable all IRQs
void pic_enable_all(void);

// Disable all IRQs
void pic_disable_all(void);

// Get the current IRQ mask
uint16_t pic_get_mask(void);

// Set the IRQ mask
void pic_set_mask(uint16_t mask);

// Disable the PIC (for APIC systems)
void pic_disable(void);

// Read the Interrupt Request Register
uint16_t pic_read_irr(void);

// Read the In-Service Register
uint16_t pic_read_isr(void);

#endif // PIC_H