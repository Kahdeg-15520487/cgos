#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// IDT Entry structure for x86_64
typedef struct {
    uint16_t offset_low;     // Lower 16 bits of handler offset
    uint16_t selector;       // Code segment selector
    uint8_t  ist;           // Interrupt Stack Table index
    uint8_t  type_attr;     // Type and attributes
    uint16_t offset_mid;     // Middle 16 bits of handler offset
    uint32_t offset_high;    // Upper 32 bits of handler offset
    uint32_t reserved;       // Reserved, must be zero
} __attribute__((packed)) idt_entry_t;

// IDT Pointer structure
typedef struct {
    uint16_t limit;          // Size of IDT - 1
    uint64_t base;           // Base address of IDT
} __attribute__((packed)) idt_ptr_t;

// Interrupt frame structure (pushed by CPU and our handlers)
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t interrupt_number;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) interrupt_frame_t;

// IDT type and attribute flags
#define IDT_TYPE_INTERRUPT_GATE  0x8E   // 32-bit interrupt gate
#define IDT_TYPE_TRAP_GATE       0x8F   // 32-bit trap gate
#define IDT_TYPE_TASK_GATE       0x85   // Task gate

// Exception numbers
#define EXCEPTION_DIVIDE_ERROR           0
#define EXCEPTION_DEBUG                  1
#define EXCEPTION_NMI                    2
#define EXCEPTION_BREAKPOINT             3
#define EXCEPTION_OVERFLOW               4
#define EXCEPTION_BOUND_RANGE_EXCEEDED   5
#define EXCEPTION_INVALID_OPCODE         6
#define EXCEPTION_DEVICE_NOT_AVAILABLE   7
#define EXCEPTION_DOUBLE_FAULT           8
#define EXCEPTION_INVALID_TSS           10
#define EXCEPTION_SEGMENT_NOT_PRESENT   11
#define EXCEPTION_STACK_SEGMENT_FAULT   12
#define EXCEPTION_GENERAL_PROTECTION    13
#define EXCEPTION_PAGE_FAULT            14
#define EXCEPTION_X87_FPU_ERROR         16
#define EXCEPTION_ALIGNMENT_CHECK       17
#define EXCEPTION_MACHINE_CHECK         18
#define EXCEPTION_SIMD_FP_EXCEPTION     19

// IRQ numbers (start at 32)
#define IRQ_BASE                        32
#define IRQ_TIMER                       (IRQ_BASE + 0)
#define IRQ_KEYBOARD                    (IRQ_BASE + 1)
#define IRQ_CASCADE                     (IRQ_BASE + 2)
#define IRQ_COM2                        (IRQ_BASE + 3)
#define IRQ_COM1                        (IRQ_BASE + 4)
#define IRQ_LPT2                        (IRQ_BASE + 5)
#define IRQ_FLOPPY                      (IRQ_BASE + 6)
#define IRQ_LPT1                        (IRQ_BASE + 7)
#define IRQ_RTC                         (IRQ_BASE + 8)
#define IRQ_FREE1                       (IRQ_BASE + 9)
#define IRQ_FREE2                       (IRQ_BASE + 10)
#define IRQ_FREE3                       (IRQ_BASE + 11)
#define IRQ_MOUSE                       (IRQ_BASE + 12)
#define IRQ_FPU                         (IRQ_BASE + 13)
#define IRQ_PRIMARY_ATA                 (IRQ_BASE + 14)
#define IRQ_SECONDARY_ATA               (IRQ_BASE + 15)

// Number of IDT entries
#define IDT_ENTRIES 256

// IDT management functions

// Initialize the Interrupt Descriptor Table
bool idt_init(void);

// Set an IDT entry
void idt_set_entry(int index, uint64_t handler, uint16_t selector, uint8_t type_attr);

// Enable interrupts
void enable_interrupts(void);

// Disable interrupts
void disable_interrupts(void);

// Check if interrupts are enabled
bool interrupts_enabled(void);

// Install the IDT (assembly function)
extern void idt_flush(uint64_t idt_ptr);

// Interrupt handler registration
typedef void (*interrupt_handler_t)(interrupt_frame_t *frame);

// Register a custom interrupt handler
void register_interrupt_handler(int interrupt_number, interrupt_handler_t handler);

// Unregister an interrupt handler
void unregister_interrupt_handler(int interrupt_number);

// Assembly interrupt stubs (defined in interrupt_asm.s)
extern void isr0(void);   // Division by zero
extern void isr1(void);   // Debug
extern void isr2(void);   // NMI
extern void isr3(void);   // Breakpoint
extern void isr4(void);   // Overflow
extern void isr5(void);   // Bound range exceeded
extern void isr6(void);   // Invalid opcode
extern void isr7(void);   // Device not available
extern void isr8(void);   // Double fault
extern void isr10(void);  // Invalid TSS
extern void isr11(void);  // Segment not present
extern void isr12(void);  // Stack segment fault
extern void isr13(void);  // General protection
extern void isr14(void);  // Page fault
extern void isr16(void);  // x87 FPU error
extern void isr17(void);  // Alignment check
extern void isr18(void);  // Machine check
extern void isr19(void);  // SIMD FP exception

// IRQ stubs
extern void irq0(void);   // Timer
extern void irq1(void);   // Keyboard
extern void irq2(void);   // Cascade
extern void irq3(void);   // COM2
extern void irq4(void);   // COM1
extern void irq5(void);   // LPT2
extern void irq6(void);   // Floppy
extern void irq7(void);   // LPT1
extern void irq8(void);   // RTC
extern void irq9(void);   // Free
extern void irq10(void);  // Free
extern void irq11(void);  // Free
extern void irq12(void);  // Mouse
extern void irq13(void);  // FPU
extern void irq14(void);  // Primary ATA
extern void irq15(void);  // Secondary ATA

#endif // IDT_H