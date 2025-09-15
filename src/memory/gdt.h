#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// GDT Entry structure for x86_64
typedef struct {
    uint16_t limit_low;      // Lower 16 bits of segment limit
    uint16_t base_low;       // Lower 16 bits of segment base
    uint8_t  base_mid;       // Middle 8 bits of segment base
    uint8_t  access;         // Access byte
    uint8_t  granularity;    // Granularity byte
    uint8_t  base_high;      // Upper 8 bits of segment base
} __attribute__((packed)) gdt_entry_t;

// GDT Pointer structure
typedef struct {
    uint16_t limit;          // Size of GDT - 1
    uint64_t base;           // Base address of GDT
} __attribute__((packed)) gdt_ptr_t;

// GDT Access byte flags
#define GDT_ACCESS_PRESENT     (1 << 7)   // Present bit
#define GDT_ACCESS_RING0       (0 << 5)   // Ring 0 (kernel)
#define GDT_ACCESS_RING1       (1 << 5)   // Ring 1
#define GDT_ACCESS_RING2       (2 << 5)   // Ring 2
#define GDT_ACCESS_RING3       (3 << 5)   // Ring 3 (user)
#define GDT_ACCESS_DESCRIPTOR  (1 << 4)   // Descriptor type (1 = code/data)
#define GDT_ACCESS_EXECUTABLE  (1 << 3)   // Executable bit (1 = code, 0 = data)
#define GDT_ACCESS_DC          (1 << 2)   // Direction/Conforming bit
#define GDT_ACCESS_RW          (1 << 1)   // Readable/Writable bit
#define GDT_ACCESS_ACCESSED    (1 << 0)   // Accessed bit

// GDT Granularity byte flags
#define GDT_GRAN_4K            (1 << 7)   // 4KB granularity
#define GDT_GRAN_32BIT         (1 << 6)   // 32-bit mode
#define GDT_GRAN_64BIT         (1 << 5)   // 64-bit mode (long mode)

// GDT segment selectors
#define GDT_KERNEL_CODE_SELECTOR   0x08   // Kernel code segment
#define GDT_KERNEL_DATA_SELECTOR   0x10   // Kernel data segment
#define GDT_USER_CODE_SELECTOR     0x18   // User code segment
#define GDT_USER_DATA_SELECTOR     0x20   // User data segment

// Number of GDT entries
#define GDT_ENTRIES 5

// GDT management functions

// Initialize the Global Descriptor Table
bool gdt_init(void);

// Get the current GDT pointer
gdt_ptr_t *gdt_get_pointer(void);

// Install the GDT (assembly function)
extern void gdt_flush(uint64_t gdt_ptr);

#endif // GDT_H