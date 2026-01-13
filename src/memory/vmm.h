#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Page size constants
#define PAGE_SIZE 4096
#define PAGE_MASK (PAGE_SIZE - 1)
#define PAGE_ALIGN(addr) ((addr) & ~PAGE_MASK)
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_MASK) & ~PAGE_MASK)

// Virtual memory layout
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ULL
#define MMIO_VIRTUAL_BASE   0xFFFFFFFFC0000000ULL
#define USER_VIRTUAL_BASE   0x0000000000400000ULL

// Page table entry flags
#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITABLE   (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_PWT        (1ULL << 3)  // Page Write-Through
#define PAGE_PCD        (1ULL << 4)  // Page Cache Disable
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_HUGE       (1ULL << 7)
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NOEXEC     (1ULL << 63)

// Legacy flag aliases for compatibility
#define PAGE_WRITETHROUGH PAGE_PWT
#define PAGE_NOCACHE      PAGE_PCD

// Page table structure
typedef struct {
    uint64_t entries[512];
} page_table_t __attribute__((aligned(4096)));

// Virtual memory context
typedef struct {
    page_table_t *pml4;
    uint64_t cr3_value;
} vmm_context_t;

// Function declarations
int vmm_init(void);
void* vmm_map_mmio(uint64_t physical_addr, size_t size);
void vmm_unmap(void *virtual_addr, size_t size);
void* vmm_map_page(uint64_t physical_addr, uint64_t virtual_addr, uint64_t flags);
void vmm_unmap_page(uint64_t virtual_addr);
uint64_t vmm_get_physical_addr(uint64_t virtual_addr);
vmm_context_t* vmm_get_current_context(void);
void vmm_switch_context(vmm_context_t *context);
void vmm_enable_paging(void);

// Helper macros
#define VIRT_TO_PHYS(vaddr) ((uint64_t)(vaddr) - KERNEL_VIRTUAL_BASE)
#define PHYS_TO_VIRT(paddr) ((void*)((uint64_t)(paddr) + KERNEL_VIRTUAL_BASE))

// HHDM (Higher Half Direct Map) support
uint64_t vmm_get_hhdm_offset(void);
void vmm_set_hhdm_offset(uint64_t offset);
#define PHYS_TO_HHDM(paddr) ((void*)((uint64_t)(paddr) + vmm_get_hhdm_offset()))

// Page table index extraction
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDP_INDEX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)
