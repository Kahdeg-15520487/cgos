#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Page size constants
#define PAGE_SIZE 4096
#define PAGE_ALIGN_MASK (PAGE_SIZE - 1)

// Virtual memory address space layout for x86_64
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000UL  // -2GB kernel space
#define USER_VIRTUAL_BASE   0x0000000000400000UL  // 4MB user space start
#define KERNEL_HEAP_BASE    0xFFFFFFFFC0000000UL  // -1GB kernel heap

// Page table entry flags
#define PTE_PRESENT     (1UL << 0)   // Page is present in memory
#define PTE_WRITABLE    (1UL << 1)   // Page is writable
#define PTE_USER        (1UL << 2)   // Page accessible by user mode
#define PTE_WRITE_THROUGH (1UL << 3) // Write-through caching
#define PTE_CACHE_DISABLE (1UL << 4) // Disable caching
#define PTE_ACCESSED    (1UL << 5)   // Page has been accessed
#define PTE_DIRTY       (1UL << 6)   // Page has been written to
#define PTE_LARGE_PAGE  (1UL << 7)   // Large page (2MB/1GB)
#define PTE_GLOBAL      (1UL << 8)   // Global page
#define PTE_NO_EXECUTE  (1UL << 63)  // No execute bit

// Page table structure for x86_64 4-level paging
typedef struct page_table {
    uint64_t entries[512];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

// Virtual memory manager structure
typedef struct {
    page_table_t *pml4;           // Page Map Level 4 (top level)
    uintptr_t kernel_pml4_phys;   // Physical address of kernel PML4
    bool paging_enabled;          // Whether paging is currently enabled
} vmm_t;

// Virtual memory manager functions

// Initialize the virtual memory manager
bool vmm_init(void);

// Get the current VMM instance
vmm_t *vmm_get_current(void);

// Map a virtual address to a physical address
bool vmm_map_page(uintptr_t virtual_addr, uintptr_t physical_addr, uint64_t flags);

// Unmap a virtual address
bool vmm_unmap_page(uintptr_t virtual_addr);

// Map multiple contiguous pages
bool vmm_map_pages(uintptr_t virtual_addr, uintptr_t physical_addr, size_t count, uint64_t flags);

// Unmap multiple contiguous pages
bool vmm_unmap_pages(uintptr_t virtual_addr, size_t count);

// Get the physical address for a virtual address
uintptr_t vmm_get_physical_address(uintptr_t virtual_addr);

// Check if a virtual address is mapped
bool vmm_is_mapped(uintptr_t virtual_addr);

// Enable paging (switch to virtual memory mode)
bool vmm_enable_paging(void);

// Identity map a physical memory region
bool vmm_identity_map_range(uintptr_t start, uintptr_t end, uint64_t flags);

// Kernel heap allocation functions
void *vmm_alloc_kernel_pages(size_t count);
void vmm_free_kernel_pages(void *virtual_addr, size_t count);

// Initial setup functions
bool vmm_setup_initial_mappings(void);
bool vmm_setup_initial_mappings_with_kernel(uintptr_t kernel_virt_base, uintptr_t kernel_phys_base);

// Utility functions
static inline uintptr_t vmm_page_align_down(uintptr_t addr) {
    return addr & ~PAGE_ALIGN_MASK;
}

static inline uintptr_t vmm_page_align_up(uintptr_t addr) {
    return (addr + PAGE_ALIGN_MASK) & ~PAGE_ALIGN_MASK;
}

static inline bool vmm_is_page_aligned(uintptr_t addr) {
    return (addr & PAGE_ALIGN_MASK) == 0;
}

// Page table index extraction macros for x86_64
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDP_INDEX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

// Extract physical address from page table entry
#define PTE_GET_ADDR(pte) ((pte) & 0x000FFFFFFFFFF000UL)

#endif // VMM_H