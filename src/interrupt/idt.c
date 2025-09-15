#include "idt.h"
#include "../debug/debug.h"
#include "../memory/memory.h"

// Global IDT table and pointer
static idt_entry_t idt_table[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

// Interrupt handlers array
static interrupt_handler_t interrupt_handlers[IDT_ENTRIES];

// Internal functions
static void default_exception_handler(interrupt_frame_t *frame);
static void default_irq_handler(interrupt_frame_t *frame);

bool idt_init(void) {
    DEBUG_INFO("Initializing Interrupt Descriptor Table");
    
    // Set up the IDT pointer
    idt_ptr.limit = sizeof(idt_table) - 1;
    idt_ptr.base = (uint64_t)&idt_table;
    
    // Clear the IDT table and handlers
    memset(idt_table, 0, sizeof(idt_table));
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));
    
    // Set up exception handlers (0-31)
    idt_set_entry(0,  (uint64_t)isr0,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(1,  (uint64_t)isr1,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(2,  (uint64_t)isr2,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(3,  (uint64_t)isr3,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(4,  (uint64_t)isr4,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(5,  (uint64_t)isr5,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(6,  (uint64_t)isr6,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(7,  (uint64_t)isr7,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(8,  (uint64_t)isr8,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(10, (uint64_t)isr10, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(11, (uint64_t)isr11, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(12, (uint64_t)isr12, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(13, (uint64_t)isr13, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(14, (uint64_t)isr14, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(16, (uint64_t)isr16, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(17, (uint64_t)isr17, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(18, (uint64_t)isr18, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(19, (uint64_t)isr19, 0x08, IDT_TYPE_INTERRUPT_GATE);
    
    // Set up IRQ handlers (32-47)
    idt_set_entry(32, (uint64_t)irq0,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(33, (uint64_t)irq1,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(34, (uint64_t)irq2,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(35, (uint64_t)irq3,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(36, (uint64_t)irq4,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(37, (uint64_t)irq5,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(38, (uint64_t)irq6,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(39, (uint64_t)irq7,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(40, (uint64_t)irq8,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(41, (uint64_t)irq9,  0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(42, (uint64_t)irq10, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(43, (uint64_t)irq11, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(44, (uint64_t)irq12, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(45, (uint64_t)irq13, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(46, (uint64_t)irq14, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_entry(47, (uint64_t)irq15, 0x08, IDT_TYPE_INTERRUPT_GATE);
    
    // Load the IDT
    idt_flush((uint64_t)&idt_ptr);
    
    DEBUG_INFO("IDT initialized and loaded successfully");
    return true;
}

void idt_set_entry(int index, uint64_t handler, uint16_t selector, uint8_t type_attr) {
    if (index >= IDT_ENTRIES) {
        return; // Invalid index
    }
    
    idt_entry_t *entry = &idt_table[index];
    
    // Set the handler address
    entry->offset_low = handler & 0xFFFF;
    entry->offset_mid = (handler >> 16) & 0xFFFF;
    entry->offset_high = (handler >> 32) & 0xFFFFFFFF;
    
    // Set selector and attributes
    entry->selector = selector;
    entry->type_attr = type_attr;
    entry->ist = 0;          // Don't use IST for now
    entry->reserved = 0;
}

void enable_interrupts(void) {
    asm volatile("sti");
}

void disable_interrupts(void) {
    asm volatile("cli");
}

bool interrupts_enabled(void) {
    uint64_t flags;
    asm volatile("pushfq; popq %0" : "=r"(flags));
    return (flags & 0x200) != 0; // Check interrupt flag
}

void register_interrupt_handler(int interrupt_number, interrupt_handler_t handler) {
    if (interrupt_number >= 0 && interrupt_number < IDT_ENTRIES) {
        interrupt_handlers[interrupt_number] = handler;
    }
}

void unregister_interrupt_handler(int interrupt_number) {
    if (interrupt_number >= 0 && interrupt_number < IDT_ENTRIES) {
        interrupt_handlers[interrupt_number] = NULL;
    }
}

// Common interrupt handler called from assembly
void interrupt_handler_common(interrupt_frame_t *frame) {
    int interrupt_number = frame->interrupt_number;
    
    // Call custom handler if registered
    if (interrupt_handlers[interrupt_number]) {
        interrupt_handlers[interrupt_number](frame);
    } else if (interrupt_number < 32) {
        // Exception
        default_exception_handler(frame);
    } else if (interrupt_number >= 32 && interrupt_number < 48) {
        // IRQ
        default_irq_handler(frame);
    }
}

// Default exception handler
static void default_exception_handler(interrupt_frame_t *frame) {
    const char *exception_names[] = {
        "Division by Zero",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "Bound Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Segment Fault",
        "General Protection Fault",
        "Page Fault",
        "Reserved",
        "x87 FPU Error",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating Point Exception"
    };
    
    const char *name = (frame->interrupt_number < 20) ? 
                       exception_names[frame->interrupt_number] : "Unknown Exception";
    
    DEBUG_ERROR("Exception %d (%s) occurred!", frame->interrupt_number, name);
    DEBUG_ERROR("Error code: 0x%lx", frame->error_code);
    DEBUG_ERROR("RIP: 0x%lx, RSP: 0x%lx", frame->rip, frame->rsp);
    DEBUG_ERROR("RAX: 0x%lx, RBX: 0x%lx, RCX: 0x%lx, RDX: 0x%lx", 
                frame->rax, frame->rbx, frame->rcx, frame->rdx);
    
    // Halt the system on exception
    asm volatile("cli; hlt");
}

// Default IRQ handler
static void default_irq_handler(interrupt_frame_t *frame) {
    DEBUG_INFO("IRQ %d received", frame->interrupt_number - 32);
    
    // Send EOI to PIC if this is a hardware interrupt
    if (frame->interrupt_number >= 32 && frame->interrupt_number < 48) {
        // Send EOI to slave PIC if IRQ >= 8
        if (frame->interrupt_number >= 40) {
            asm volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
        }
        // Send EOI to master PIC
        asm volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
    }
}