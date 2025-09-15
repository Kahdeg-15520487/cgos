#include "gdt.h"
#include "../debug/debug.h"
#include "memory.h"

// Global GDT table and pointer
static gdt_entry_t gdt_table[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr;

// Internal helper function to set a GDT entry
static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity);

bool gdt_init(void) {
    DEBUG_INFO("Initializing Global Descriptor Table");
    
    // Set up the GDT pointer
    gdt_ptr.limit = sizeof(gdt_table) - 1;
    gdt_ptr.base = (uint64_t)&gdt_table;
    
    // Clear the GDT table
    memset(gdt_table, 0, sizeof(gdt_table));
    
    // Set up GDT entries
    // Entry 0: Null descriptor (required)
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Entry 1: Kernel code segment (64-bit)
    gdt_set_entry(1, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DESCRIPTOR | 
                  GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                  GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // Entry 2: Kernel data segment (64-bit)
    gdt_set_entry(2, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DESCRIPTOR | 
                  GDT_ACCESS_RW,
                  GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // Entry 3: User code segment (64-bit)
    gdt_set_entry(3, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DESCRIPTOR | 
                  GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                  GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // Entry 4: User data segment (64-bit)
    gdt_set_entry(4, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DESCRIPTOR | 
                  GDT_ACCESS_RW,
                  GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // Load the GDT
    gdt_flush((uint64_t)&gdt_ptr);
    
    DEBUG_INFO("GDT initialized and loaded successfully");
    return true;
}

gdt_ptr_t *gdt_get_pointer(void) {
    return &gdt_ptr;
}

// Internal helper function to set a GDT entry
static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    if (index >= GDT_ENTRIES) {
        return; // Invalid index
    }
    
    gdt_entry_t *entry = &gdt_table[index];
    
    // Set the base address
    entry->base_low = base & 0xFFFF;
    entry->base_mid = (base >> 16) & 0xFF;
    entry->base_high = (base >> 24) & 0xFF;
    
    // Set the limit
    entry->limit_low = limit & 0xFFFF;
    entry->granularity = (limit >> 16) & 0x0F;
    
    // Set the granularity and access flags
    entry->granularity |= granularity & 0xF0;
    entry->access = access;
}