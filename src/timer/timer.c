#include "timer.h"
#include "../debug/debug.h"
#include "../interrupt/interrupt.h"

// Global tick counter
static volatile uint64_t ticks = 0;

// Initialize the 8259 PIC (Programmable Interrupt Controller)
void pic_init(void) {
    DEBUG_INFO("Initializing PIC...\n");
    
    // Save masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // Start initialization sequence (ICW1)
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // ICW2: Set vector offsets (remap IRQs)
    // PIC1: IRQ 0-7 -> vectors 32-39
    // PIC2: IRQ 8-15 -> vectors 40-47
    outb(PIC1_DATA, 0x20);  // IRQ 0-7 start at vector 32
    io_wait();
    outb(PIC2_DATA, 0x28);  // IRQ 8-15 start at vector 40
    io_wait();
    
    // ICW3: Tell master PIC that slave is at IRQ2
    outb(PIC1_DATA, 0x04);  // Slave at IRQ2
    io_wait();
    outb(PIC2_DATA, 0x02);  // Slave identity
    io_wait();
    
    // ICW4: Set 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore masks (mask all except timer initially)
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
    
    DEBUG_INFO("PIC initialized, IRQs remapped to vectors 32-47\n");
}

// Send End Of Interrupt to PIC
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

// Mask (disable) an IRQ
void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

// Unmask (enable) an IRQ
void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

// Timer interrupt handler (called from assembly stub)
void timer_irq_handler(void) {
    ticks++;
    
    // Send EOI to PIC (inline to minimize overhead)
    outb(PIC1_COMMAND, PIC_EOI);
}

// Initialize the PIT (Programmable Interval Timer)
void timer_init(void) {
    DEBUG_INFO("Initializing timer system...\n");
    
    // First, initialize the PIC
    pic_init();
    
    // Calculate divisor for desired frequency
    uint16_t divisor = PIT_FREQUENCY / TIMER_FREQUENCY_HZ;
    
    DEBUG_INFO("PIT divisor: %d for %d Hz\n", divisor, TIMER_FREQUENCY_HZ);
    
    // Send command byte: Channel 0, Access mode lo/hi, Mode 3 (square wave)
    outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LOHI | PIT_CMD_MODE_SQUARE | PIT_CMD_BINARY);
    
    // Send divisor (low byte first, then high byte)
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));
    
    // Unmask timer IRQ (IRQ0)
    pic_clear_mask(IRQ_TIMER);
    
    // Enable interrupts
    __asm__ volatile("sti");
    
    DEBUG_INFO("Timer initialized at %d Hz, interrupts enabled\n", TIMER_FREQUENCY_HZ);
}

// Get current tick count
uint64_t timer_get_ticks(void) {
    return ticks;
}

// Get elapsed seconds since boot
uint32_t timer_get_seconds(void) {
    return (uint32_t)(ticks / TIMER_FREQUENCY_HZ);
}

// Sleep for a given number of milliseconds (busy wait)
void timer_sleep_ms(uint32_t ms) {
    uint64_t target = ticks + ms;  // At 1000 Hz, 1 tick = 1 ms
    while (ticks < target) {
        __asm__ volatile("pause");  // Hint to CPU we're in a spin loop
    }
}
