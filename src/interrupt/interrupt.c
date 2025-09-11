#include "interrupt.h"
#include "../debug/debug.h"
#include "../memory/vmm.h"
#include <string.h>

// IDT with 256 entries
static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

// Set up an IDT gate
void idt_set_gate(int num, uint64_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = type_attr;
    idt[num].reserved = 0;
}

// Initialize the interrupt system
void interrupt_init(void) {
    DEBUG_INFO("Initializing interrupt system\n");
    
    // Clear the IDT
    memset(idt, 0, sizeof(idt));
    
    // Set up the IDT pointer
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)idt;
    
    // Set up exception handlers
    idt_set_gate(EXCEPTION_DIVIDE_ERROR, (uint64_t)exception_handler_0, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_DEBUG, (uint64_t)exception_handler_1, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_NMI, (uint64_t)exception_handler_2, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_BREAKPOINT, (uint64_t)exception_handler_3, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_OVERFLOW, (uint64_t)exception_handler_4, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_BOUND_RANGE, (uint64_t)exception_handler_5, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_INVALID_OPCODE, (uint64_t)exception_handler_6, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_DEVICE_NOT_AVAILABLE, (uint64_t)exception_handler_7, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_DOUBLE_FAULT, (uint64_t)exception_handler_8, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_INVALID_TSS, (uint64_t)exception_handler_10, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_SEGMENT_NOT_PRESENT, (uint64_t)exception_handler_11, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_STACK_FAULT, (uint64_t)exception_handler_12, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_GENERAL_PROTECTION, (uint64_t)exception_handler_13, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_PAGE_FAULT, (uint64_t)exception_handler_14, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_FLOATING_POINT, (uint64_t)exception_handler_16, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_ALIGNMENT_CHECK, (uint64_t)exception_handler_17, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_MACHINE_CHECK, (uint64_t)exception_handler_18, 0x08, IDT_TYPE_INTERRUPT_GATE);
    idt_set_gate(EXCEPTION_SIMD_FLOATING_POINT, (uint64_t)exception_handler_19, 0x08, IDT_TYPE_INTERRUPT_GATE);
    
    // Load the IDT
    asm volatile("lidt %0" :: "m"(idt_ptr));
    
    DEBUG_INFO("IDT loaded with %d entries at 0x%lx\n", 256, (uint64_t)idt);
    DEBUG_INFO("Interrupt system initialization completed\n");
}

// Page fault handler
void page_fault_handler(interrupt_frame_with_error_t *frame) {
    // Get the faulting address from CR2
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    uint64_t error_code = frame->error_code;
    
    DEBUG_ERROR("Page Fault!\n");
    DEBUG_ERROR("  Fault Address: 0x%lx\n", fault_addr);
    DEBUG_ERROR("  Error Code: 0x%lx\n", error_code);
    DEBUG_ERROR("  RIP: 0x%lx\n", frame->rip);
    DEBUG_ERROR("  RSP: 0x%lx\n", frame->rsp);
    
    // Decode error code
    DEBUG_ERROR("  Fault Type:\n");
    if (error_code & PAGE_FAULT_PRESENT) {
        DEBUG_ERROR("    - Page protection violation\n");
    } else {
        DEBUG_ERROR("    - Page not present\n");
    }
    
    if (error_code & PAGE_FAULT_WRITE) {
        DEBUG_ERROR("    - Write access\n");
    } else {
        DEBUG_ERROR("    - Read access\n");
    }
    
    if (error_code & PAGE_FAULT_USER) {
        DEBUG_ERROR("    - User mode access\n");
    } else {
        DEBUG_ERROR("    - Kernel mode access\n");
    }
    
    if (error_code & PAGE_FAULT_RESERVED) {
        DEBUG_ERROR("    - Reserved bit violation\n");
    }
    
    if (error_code & PAGE_FAULT_INSTRUCTION) {
        DEBUG_ERROR("    - Instruction fetch\n");
    }
    
    // Try to handle MMIO access
    if (!handle_mmio_page_fault(fault_addr, error_code)) {
        DEBUG_ERROR("Failed to handle page fault - halting system\n");
        
        // Halt the system
        asm volatile("cli; hlt");
        while (1) {
            asm volatile("hlt");
        }
    }
}

// Try to handle MMIO page faults by mapping the page
bool handle_mmio_page_fault(uint64_t fault_addr, uint64_t error_code) {
    // Check if this is an MMIO address range (typically high physical addresses)
    if (fault_addr >= 0xE0000000 && fault_addr < 0x100000000) {
        DEBUG_INFO("Attempting to map MMIO page at 0x%lx\n", fault_addr);
        
        // Align to page boundary
        uint64_t page_addr = fault_addr & ~PAGE_MASK;
        
        // Try to map the page with appropriate flags
        uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE;
        if (!(error_code & PAGE_FAULT_USER)) {
            // Kernel access - don't set user flag
        } else {
            flags |= PAGE_USER;
        }
        
        // Add cache disable for MMIO
        flags |= PAGE_PCD | PAGE_PWT;
        
        void *mapped = vmm_map_page(page_addr, page_addr, flags);
        if (mapped) {
            DEBUG_INFO("Successfully mapped MMIO page at 0x%lx\n", page_addr);
            return true;
        } else {
            DEBUG_ERROR("Failed to map MMIO page at 0x%lx\n", page_addr);
        }
    }
    
    return false;
}

// Generic exception handler (called from assembly stubs)
void generic_exception_handler(int exception_num, interrupt_frame_t *frame) {
    DEBUG_ERROR("Exception %d occurred!\n", exception_num);
    DEBUG_ERROR("  RIP: 0x%lx\n", frame->rip);
    DEBUG_ERROR("  CS: 0x%lx\n", frame->cs);
    DEBUG_ERROR("  RFLAGS: 0x%lx\n", frame->rflags);
    DEBUG_ERROR("  RSP: 0x%lx\n", frame->rsp);
    DEBUG_ERROR("  SS: 0x%lx\n", frame->ss);
    
    // Halt the system for unhandled exceptions
    DEBUG_ERROR("Unhandled exception - halting system\n");
    asm volatile("cli; hlt");
    while (1) {
        asm volatile("hlt");
    }
}

// Generic exception handler with error code
void generic_exception_handler_with_error(int exception_num, interrupt_frame_with_error_t *frame) {
    if (exception_num == EXCEPTION_PAGE_FAULT) {
        page_fault_handler(frame);
        return;
    }
    
    DEBUG_ERROR("Exception %d occurred!\n", exception_num);
    DEBUG_ERROR("  Error Code: 0x%lx\n", frame->error_code);
    DEBUG_ERROR("  RIP: 0x%lx\n", frame->rip);
    DEBUG_ERROR("  CS: 0x%lx\n", frame->cs);
    DEBUG_ERROR("  RFLAGS: 0x%lx\n", frame->rflags);
    DEBUG_ERROR("  RSP: 0x%lx\n", frame->rsp);
    DEBUG_ERROR("  SS: 0x%lx\n", frame->ss);
    
    // Halt the system for unhandled exceptions
    DEBUG_ERROR("Unhandled exception - halting system\n");
    asm volatile("cli; hlt");
    while (1) {
        asm volatile("hlt");
    }
}
