#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// GDT segment selectors (byte offsets into GDT)
// These are used with segment registers and for iret stack frames
#define GDT_NULL_SELECTOR       0x00
#define GDT_KERNEL_CODE         0x08    // Ring 0 64-bit code segment
#define GDT_KERNEL_DATA         0x10    // Ring 0 data segment
#define GDT_USER_CODE           0x18    // Ring 3 64-bit code segment
#define GDT_USER_DATA           0x20    // Ring 3 data segment
#define GDT_TSS                 0x28    // TSS descriptor (16 bytes in long mode)

// Selector with RPL (Requested Privilege Level)
#define GDT_USER_CODE_RPL3      (GDT_USER_CODE | 3)
#define GDT_USER_DATA_RPL3      (GDT_USER_DATA | 3)

// GDT access byte flags
#define GDT_ACCESS_PRESENT      (1 << 7)    // Segment is present
#define GDT_ACCESS_DPL0         (0 << 5)    // Ring 0 (kernel)
#define GDT_ACCESS_DPL3         (3 << 5)    // Ring 3 (user)
#define GDT_ACCESS_SEGMENT      (1 << 4)    // Code/Data segment (not system)
#define GDT_ACCESS_EXECUTABLE   (1 << 3)    // Executable (code segment)
#define GDT_ACCESS_DC           (1 << 2)    // Direction/Conforming
#define GDT_ACCESS_RW           (1 << 1)    // Readable (code) / Writable (data)
#define GDT_ACCESS_ACCESSED     (1 << 0)    // CPU sets this when accessed

// GDT flags (upper 4 bits of flags_limit_high)
#define GDT_FLAG_GRANULARITY    (1 << 7)    // 4KB granularity
#define GDT_FLAG_SIZE           (1 << 6)    // 32-bit (0 for 64-bit code)
#define GDT_FLAG_LONG           (1 << 5)    // 64-bit code segment

// TSS access byte
#define TSS_ACCESS_PRESENT      (1 << 7)
#define TSS_ACCESS_DPL0         (0 << 5)
#define TSS_ACCESS_TYPE_TSS     0x09        // 64-bit TSS (Available)

// Standard GDT entry (8 bytes)
typedef struct {
    uint16_t limit_low;         // Limit bits 0-15
    uint16_t base_low;          // Base bits 0-15
    uint8_t  base_mid;          // Base bits 16-23
    uint8_t  access;            // Access byte
    uint8_t  flags_limit_high;  // Flags (4 bits) | Limit bits 16-19 (4 bits)
    uint8_t  base_high;         // Base bits 24-31
} __attribute__((packed)) gdt_entry_t;

// TSS descriptor in long mode (16 bytes - takes 2 GDT slots)
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
    uint32_t base_upper;        // Upper 32 bits of base address
    uint32_t reserved;          // Must be zero
} __attribute__((packed)) tss_descriptor_t;

// Task State Segment for x86-64 (104 bytes minimum)
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;              // Stack pointer for Ring 0 (CRITICAL for interrupts)
    uint64_t rsp1;              // Stack pointer for Ring 1 (unused)
    uint64_t rsp2;              // Stack pointer for Ring 2 (unused)
    uint64_t reserved1;
    uint64_t ist1;              // Interrupt Stack Table entry 1
    uint64_t ist2;              // Interrupt Stack Table entry 2
    uint64_t ist3;              // Interrupt Stack Table entry 3
    uint64_t ist4;              // Interrupt Stack Table entry 4
    uint64_t ist5;              // Interrupt Stack Table entry 5
    uint64_t ist6;              // Interrupt Stack Table entry 6
    uint64_t ist7;              // Interrupt Stack Table entry 7
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;       // I/O Permission Bitmap offset
} __attribute__((packed)) tss_t;

// GDT pointer structure for lgdt instruction
typedef struct {
    uint16_t limit;             // Size of GDT - 1
    uint64_t base;              // Base address of GDT
} __attribute__((packed)) gdt_ptr_t;

// Function declarations
void gdt_init(void);
void gdt_set_kernel_stack(uint64_t stack);
tss_t *gdt_get_tss(void);

#endif // GDT_H
