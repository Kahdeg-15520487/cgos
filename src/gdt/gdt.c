#include "gdt.h"
#include "../debug/debug.h"
#include <string.h>

// Our GDT with 7 entries (null + 4 segments + TSS which uses 2 slots)
// Entry 0: Null
// Entry 1: Kernel Code (0x08)
// Entry 2: Kernel Data (0x10)
// Entry 3: User Code (0x18)
// Entry 4: User Data (0x20)
// Entry 5-6: TSS (0x28) - 16 bytes, spans 2 entries
static struct {
    gdt_entry_t entries[5];
    tss_descriptor_t tss_desc;
} __attribute__((packed, aligned(16))) gdt;

// The TSS itself
static tss_t tss __attribute__((aligned(16)));

// GDT pointer for lgdt
static gdt_ptr_t gdt_ptr;

// Helper to set a standard GDT entry
static void gdt_set_entry(int index, uint32_t base, uint32_t limit, 
                          uint8_t access, uint8_t flags) {
    gdt.entries[index].limit_low = limit & 0xFFFF;
    gdt.entries[index].base_low = base & 0xFFFF;
    gdt.entries[index].base_mid = (base >> 16) & 0xFF;
    gdt.entries[index].access = access;
    gdt.entries[index].flags_limit_high = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt.entries[index].base_high = (base >> 24) & 0xFF;
}

// Set up the TSS descriptor (16 bytes in long mode)
static void gdt_set_tss(uint64_t tss_addr, uint32_t tss_size) {
    gdt.tss_desc.limit_low = tss_size & 0xFFFF;
    gdt.tss_desc.base_low = tss_addr & 0xFFFF;
    gdt.tss_desc.base_mid = (tss_addr >> 16) & 0xFF;
    gdt.tss_desc.access = TSS_ACCESS_PRESENT | TSS_ACCESS_DPL0 | TSS_ACCESS_TYPE_TSS;
    gdt.tss_desc.flags_limit_high = ((tss_size >> 16) & 0x0F);
    gdt.tss_desc.base_high = (tss_addr >> 24) & 0xFF;
    gdt.tss_desc.base_upper = (tss_addr >> 32) & 0xFFFFFFFF;
    gdt.tss_desc.reserved = 0;
}

// External assembly function to load GDT and reload segment registers
extern void gdt_load(gdt_ptr_t *ptr, uint16_t code_selector, uint16_t data_selector);

void gdt_init(void) {
    DEBUG_INFO("Initializing GDT...\n");
    
    // Clear everything
    memset(&gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));
    
    // Entry 0: Null descriptor (required)
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Entry 1: Kernel Code Segment (0x08)
    // Access: Present, Ring 0, Code/Data segment, Executable, Readable
    // Flags: Long mode (64-bit)
    gdt_set_entry(1, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_SEGMENT | 
        GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
        GDT_FLAG_LONG | GDT_FLAG_GRANULARITY);
    
    // Entry 2: Kernel Data Segment (0x10)
    // Access: Present, Ring 0, Code/Data segment, Writable
    // Flags: Granularity (for consistency, though ignored for data in long mode)
    gdt_set_entry(2, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_SEGMENT | GDT_ACCESS_RW,
        GDT_FLAG_GRANULARITY);
    
    // Entry 3: User Code Segment (0x18)
    // Access: Present, Ring 3, Code/Data segment, Executable, Readable
    // Flags: Long mode (64-bit)
    gdt_set_entry(3, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_SEGMENT | 
        GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
        GDT_FLAG_LONG | GDT_FLAG_GRANULARITY);
    
    // Entry 4: User Data Segment (0x20)
    // Access: Present, Ring 3, Code/Data segment, Writable
    gdt_set_entry(4, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_SEGMENT | GDT_ACCESS_RW,
        GDT_FLAG_GRANULARITY);
    
    // Set up TSS
    tss.iopb_offset = sizeof(tss_t);  // No I/O permission bitmap
    // RSP0 will be set later when we have a kernel stack for the current task
    
    // Entry 5-6: TSS descriptor (at offset 0x28)
    gdt_set_tss((uint64_t)&tss, sizeof(tss_t) - 1);
    
    // Set up GDT pointer
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    DEBUG_INFO("GDT at 0x%lx, size %d bytes\n", gdt_ptr.base, gdt_ptr.limit + 1);
    DEBUG_INFO("TSS at 0x%lx, size %d bytes\n", (uint64_t)&tss, sizeof(tss_t));
    
    // Load the GDT
    gdt_load(&gdt_ptr, GDT_KERNEL_CODE, GDT_KERNEL_DATA);
    
    // Load TSS (TR register)
    uint16_t tss_selector = GDT_TSS;
    __asm__ volatile("ltr %0" : : "r"(tss_selector));
    
    DEBUG_INFO("GDT and TSS loaded successfully\n");
}

void gdt_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}

tss_t *gdt_get_tss(void) {
    return &tss;
}
