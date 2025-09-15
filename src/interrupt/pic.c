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

// I/O delay function
static inline void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

// Current PIC configuration
static uint8_t master_mask = 0xFF;
static uint8_t slave_mask = 0xFF;
static uint8_t master_offset = PIC1_OFFSET;
static uint8_t slave_offset = PIC2_OFFSET;

bool pic_init(uint8_t master_off, uint8_t slave_off) {
    DEBUG_INFO("Initializing PIC with master offset %d, slave offset %d", master_off, slave_off);
    
    master_offset = master_off;
    slave_offset = slave_off;
    
    // Save current masks (unused but kept for potential future use)
    (void)inb(PIC1_DATA);  // old_master_mask
    (void)inb(PIC2_DATA);  // old_slave_mask
    
    // Start initialization sequence in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // Set interrupt vector offsets
    outb(PIC1_DATA, master_offset);
    io_wait();
    outb(PIC2_DATA, slave_offset);
    io_wait();
    
    // Configure cascade: tell master PIC that there's a slave PIC at IRQ2
    outb(PIC1_DATA, 4);  // 0000 0100 (bit 2 set for IRQ2)
    io_wait();
    // Tell slave PIC its cascade identity
    outb(PIC2_DATA, 2);  // Cascade identity
    io_wait();
    
    // Set mode to 8086
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore or set initial masks (disable all IRQs initially)
    master_mask = 0xFF;
    slave_mask = 0xFF;
    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
    
    DEBUG_INFO("PIC initialization completed successfully");
    return true;
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        // Send EOI to slave PIC
        outb(PIC2_COMMAND, PIC_EOI);
    }
    // Always send EOI to master PIC
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // Master PIC
        port = PIC1_DATA;
        master_mask &= ~(1 << irq);
        value = master_mask;
    } else {
        // Slave PIC
        port = PIC2_DATA;
        irq -= 8;
        slave_mask &= ~(1 << irq);
        value = slave_mask;
        
        // Also enable cascade IRQ on master if not already enabled
        master_mask &= ~(1 << PIC_IRQ_CASCADE);
        outb(PIC1_DATA, master_mask);
    }
    
    outb(port, value);
    DEBUG_INFO("Enabled IRQ %d", irq + (port == PIC2_DATA ? 8 : 0));
}

void pic_disable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // Master PIC
        port = PIC1_DATA;
        master_mask |= (1 << irq);
        value = master_mask;
    } else {
        // Slave PIC
        port = PIC2_DATA;
        irq -= 8;
        slave_mask |= (1 << irq);
        value = slave_mask;
    }
    
    outb(port, value);
    DEBUG_INFO("Disabled IRQ %d", irq + (port == PIC2_DATA ? 8 : 0));
}

void pic_enable_all(void) {
    master_mask = 0x00;
    slave_mask = 0x00;
    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
    DEBUG_INFO("Enabled all IRQs");
}

void pic_disable_all(void) {
    master_mask = 0xFF;
    slave_mask = 0xFF;
    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
    DEBUG_INFO("Disabled all IRQs");
}

uint16_t pic_get_mask(void) {
    return (inb(PIC2_DATA) << 8) | inb(PIC1_DATA);
}

void pic_set_mask(uint16_t mask) {
    master_mask = mask & 0xFF;
    slave_mask = (mask >> 8) & 0xFF;
    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

void pic_disable(void) {
    DEBUG_INFO("Disabling PIC");
    
    // Mask all interrupts
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    
    // Send ICW1 with bit 4 clear (no initialization)
    outb(PIC1_COMMAND, 0x00);
    outb(PIC2_COMMAND, 0x00);
}

uint16_t pic_read_irr(void) {
    // Read Interrupt Request Register
    outb(PIC1_COMMAND, 0x0A);  // OCW3: read IRR
    outb(PIC2_COMMAND, 0x0A);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

uint16_t pic_read_isr(void) {
    // Read In-Service Register
    outb(PIC1_COMMAND, 0x0B);  // OCW3: read ISR
    outb(PIC2_COMMAND, 0x0B);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}