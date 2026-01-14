#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// IDT entry structure
typedef struct idt_entry {
    uint16_t offset_low;    // Lower 16 bits of handler function address
    uint16_t selector;      // Code segment selector
    uint8_t  ist;          // Interrupt Stack Table offset (bits 0-2), rest reserved
    uint8_t  type_attr;    // Type and attributes
    uint16_t offset_mid;    // Middle 16 bits of handler function address
    uint32_t offset_high;   // Higher 32 bits of handler function address
    uint32_t reserved;      // Reserved, must be zero
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct idt_ptr {
    uint16_t limit;    // Size of IDT - 1
    uint64_t base;     // Base address of IDT
} __attribute__((packed)) idt_ptr_t;

// Exception/interrupt numbers
#define EXCEPTION_DIVIDE_ERROR          0
#define EXCEPTION_DEBUG                 1
#define EXCEPTION_NMI                   2
#define EXCEPTION_BREAKPOINT            3
#define EXCEPTION_OVERFLOW              4
#define EXCEPTION_BOUND_RANGE           5
#define EXCEPTION_INVALID_OPCODE        6
#define EXCEPTION_DEVICE_NOT_AVAILABLE  7
#define EXCEPTION_DOUBLE_FAULT          8
#define EXCEPTION_COPROCESSOR_SEGMENT   9
#define EXCEPTION_INVALID_TSS           10
#define EXCEPTION_SEGMENT_NOT_PRESENT   11
#define EXCEPTION_STACK_FAULT           12
#define EXCEPTION_GENERAL_PROTECTION    13
#define EXCEPTION_PAGE_FAULT            14
#define EXCEPTION_FLOATING_POINT        16
#define EXCEPTION_ALIGNMENT_CHECK       17
#define EXCEPTION_MACHINE_CHECK         18
#define EXCEPTION_SIMD_FLOATING_POINT   19
#define EXCEPTION_VIRTUALIZATION        20
#define EXCEPTION_SECURITY              30

// IDT type and attribute flags
#define IDT_TYPE_INTERRUPT_GATE 0x8E
#define IDT_TYPE_TRAP_GATE      0x8F
#define IDT_TYPE_TASK_GATE      0x85

// Interrupt frame structure (pushed by CPU)
typedef struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_t;

// Extended interrupt frame with error code
typedef struct interrupt_frame_with_error {
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_with_error_t;

// Page fault error code bits
#define PAGE_FAULT_PRESENT     (1 << 0)  // Page was present
#define PAGE_FAULT_WRITE       (1 << 1)  // Was a write operation
#define PAGE_FAULT_USER        (1 << 2)  // Fault in user mode
#define PAGE_FAULT_RESERVED    (1 << 3)  // Reserved bit violation
#define PAGE_FAULT_INSTRUCTION (1 << 4)  // Instruction fetch

// Function declarations
void interrupt_init(void);
void idt_set_gate(int num, uint64_t handler, uint16_t selector, uint8_t type_attr);
void page_fault_handler(interrupt_frame_with_error_t *frame);
bool handle_mmio_page_fault(uint64_t fault_addr, uint64_t error_code);
void generic_exception_handler(int exception_num, interrupt_frame_t *frame);
void generic_exception_handler_with_error(int exception_num, interrupt_frame_with_error_t *frame);

// Exception handler declarations
extern void exception_handler_0(void);   // Divide Error
extern void exception_handler_1(void);   // Debug
extern void exception_handler_2(void);   // NMI
extern void exception_handler_3(void);   // Breakpoint
extern void exception_handler_4(void);   // Overflow
extern void exception_handler_5(void);   // Bound Range
extern void exception_handler_6(void);   // Invalid Opcode
extern void exception_handler_7(void);   // Device Not Available
extern void exception_handler_8(void);   // Double Fault
extern void exception_handler_10(void);  // Invalid TSS
extern void exception_handler_11(void);  // Segment Not Present
extern void exception_handler_12(void);  // Stack Fault
extern void exception_handler_13(void);  // General Protection
extern void exception_handler_14(void);  // Page Fault
extern void exception_handler_16(void);  // Floating Point
extern void exception_handler_17(void);  // Alignment Check
extern void exception_handler_18(void);  // Machine Check
extern void exception_handler_19(void);  // SIMD Floating Point

// IRQ handlers (hardware interrupts)
extern void irq_handler_0(void);  // Timer (IRQ0, vector 32)
extern void irq_handler_1(void);  // Keyboard (IRQ1, vector 33)

#endif // INTERRUPT_H
