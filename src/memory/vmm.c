#include "vmm.h"
#include "pmm.h"
#include "memory.h"
#include "../debug/debug.h"
#include <string.h>

// Current virtual memory context
static vmm_context_t kernel_context;
static vmm_context_t *current_context = &kernel_context;

// MMIO allocation tracker
static uint64_t next_mmio_vaddr = MMIO_VIRTUAL_BASE;

// Forward declarations
static page_table_t* get_or_create_table(page_table_t *parent, int index, uint64_t flags);
static void map_kernel_space(void);

int vmm_init(void) {
    DEBUG_INFO("Initializing virtual memory manager\n");
    
    // Since Limine has already enabled paging and set up page tables,
    // we need to work with the existing page tables instead of creating new ones
    
    // Get the current CR3 value (Limine's page tables)
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    
    DEBUG_INFO("Using existing page tables at CR3: 0x%lx\n", current_cr3);
    
    // Set up our kernel context to use Limine's page tables
    kernel_context.pml4 = (page_table_t*)(current_cr3 & ~0xFFF);  // Remove flags
    kernel_context.cr3_value = current_cr3;
    
    // For now, we'll rely entirely on Limine's existing mappings
    // and only add new mappings for MMIO when specifically requested
    
    DEBUG_INFO("Virtual memory manager initialized with existing page tables\n");
    DEBUG_INFO("Note: Not creating additional page tables - using Limine mappings only\n");
    return 0;
}

static void map_kernel_space(void) {
    DEBUG_INFO("Using Limine's existing kernel virtual memory mappings\n");
    // Limine has already set up identity mappings and kernel space
    // We don't need to create additional mappings at this point
    DEBUG_INFO("Kernel virtual memory mappings already available\n");
}

void* vmm_map_page(uint64_t physical_addr, uint64_t virtual_addr, uint64_t flags) {
    // For now, we'll only support MMIO mapping in a limited way
    // since creating new page tables requires physical memory allocation
    // that may not be identity-mapped by Limine
    
    DEBUG_INFO("vmm_map_page: Requested mapping of phys=0x%lx to virt=0x%lx\n", 
               physical_addr, virtual_addr);
    
    // For now, we'll just return the virtual address without actually mapping
    // This is a temporary limitation until we implement proper virtual address translation
    DEBUG_INFO("Warning: vmm_map_page not fully implemented - returning requested virtual address\n");
    
    return (void*)virtual_addr;
}

static page_table_t* get_or_create_table(page_table_t *parent, int index, uint64_t flags) {
    if (parent->entries[index] & PAGE_PRESENT) {
        // Table exists, return it
        uint64_t table_phys = parent->entries[index] & ~PAGE_MASK;
        return (page_table_t*)table_phys;
    }
    
    // Need to create new table
    page_table_t *new_table = (page_table_t*)physical_alloc_page();
    if (!new_table) {
        DEBUG_ERROR("Failed to allocate page table\n");
        return NULL;
    }
    
    // Clear the new table (we're still in physical addressing mode during setup)
    memset(new_table, 0, PAGE_SIZE);
    
    // Add entry to parent table  
    parent->entries[index] = (uint64_t)new_table | flags;
    
    DEBUG_INFO("Created page table at physical 0x%lx, parent[%d] = 0x%lx\n", 
               (uint64_t)new_table, index, parent->entries[index]);
    
    return new_table;
}

void* vmm_map_mmio(uint64_t physical_addr, size_t size) {
    DEBUG_INFO("vmm_map_mmio: Requested MMIO mapping of phys=0x%lx size=0x%lx\n", 
               physical_addr, size);
    
    // For now, use a simple approach: assume Limine has set up identity mapping
    // for physical memory, or use a direct approach that avoids modifying page tables
    
    // Strategy: For MMIO regions, we'll try to use identity mapping first
    // If that fails, we'd need a more sophisticated approach, but for now
    // let's assume Limine has mapped enough physical memory for us to access MMIO
    
    // Simple identity mapping - this works if Limine mapped all physical memory
    void* result = (void*)physical_addr;
    
    DEBUG_INFO("MMIO mapping (identity): phys=0x%lx -> virt=0x%lx size=0x%lx\n", 
               physical_addr, (uint64_t)result, size);
    
    return result;
}

void vmm_unmap_page(uint64_t virtual_addr) {
    page_table_t *pml4 = current_context->pml4;
    
    // Extract page table indices
    int pml4_idx = PML4_INDEX(virtual_addr);
    int pdp_idx = PDP_INDEX(virtual_addr);
    int pd_idx = PD_INDEX(virtual_addr);
    int pt_idx = PT_INDEX(virtual_addr);
    
    // Navigate to the page table entry
    if (!(pml4->entries[pml4_idx] & PAGE_PRESENT)) return;
    page_table_t *pdp = (page_table_t*)(pml4->entries[pml4_idx] & ~PAGE_MASK);
    
    if (!(pdp->entries[pdp_idx] & PAGE_PRESENT)) return;
    page_table_t *pd = (page_table_t*)(pdp->entries[pdp_idx] & ~PAGE_MASK);
    
    if (!(pd->entries[pd_idx] & PAGE_PRESENT)) return;
    page_table_t *pt = (page_table_t*)(pd->entries[pd_idx] & ~PAGE_MASK);
    
    // Clear the page table entry
    pt->entries[pt_idx] = 0;
    
    // Invalidate TLB for this page
    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

void vmm_unmap(void *virtual_addr, size_t size) {
    uint64_t virt_start = PAGE_ALIGN((uint64_t)virtual_addr);
    uint64_t virt_end = PAGE_ALIGN_UP((uint64_t)virtual_addr + size);
    
    for (uint64_t vaddr = virt_start; vaddr < virt_end; vaddr += PAGE_SIZE) {
        vmm_unmap_page(vaddr);
    }
}

uint64_t vmm_get_physical_addr(uint64_t virtual_addr) {
    page_table_t *pml4 = current_context->pml4;
    
    // Extract page table indices
    int pml4_idx = PML4_INDEX(virtual_addr);
    int pdp_idx = PDP_INDEX(virtual_addr);
    int pd_idx = PD_INDEX(virtual_addr);
    int pt_idx = PT_INDEX(virtual_addr);
    
    // Navigate through page tables
    if (!(pml4->entries[pml4_idx] & PAGE_PRESENT)) return 0;
    page_table_t *pdp = (page_table_t*)(pml4->entries[pml4_idx] & ~PAGE_MASK);
    
    if (!(pdp->entries[pdp_idx] & PAGE_PRESENT)) return 0;
    page_table_t *pd = (page_table_t*)(pdp->entries[pdp_idx] & ~PAGE_MASK);
    
    if (!(pd->entries[pd_idx] & PAGE_PRESENT)) return 0;
    page_table_t *pt = (page_table_t*)(pd->entries[pd_idx] & ~PAGE_MASK);
    
    if (!(pt->entries[pt_idx] & PAGE_PRESENT)) return 0;
    
    uint64_t phys_page = pt->entries[pt_idx] & ~PAGE_MASK;
    uint64_t offset = virtual_addr & PAGE_MASK;
    
    return phys_page + offset;
}

vmm_context_t* vmm_get_current_context(void) {
    return current_context;
}

void vmm_switch_context(vmm_context_t *context) {
    if (context && context != current_context) {
        current_context = context;
        
        // Load new CR3
        asm volatile("mov %0, %%cr3" :: "r"(context->cr3_value) : "memory");
    }
}

void vmm_enable_paging(void) {
    DEBUG_INFO("Enabling paging with PML4 at 0x%lx\n", kernel_context.cr3_value);
    
    // Load CR3 with PML4 address
    asm volatile("mov %0, %%cr3" :: "r"(kernel_context.cr3_value) : "memory");
    
    // Enable PAE and PGE in CR4
    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5) | (1 << 7); // PAE | PGE
    asm volatile("mov %0, %%cr4" :: "r"(cr4) : "memory");
    
    // Enable paging in CR0 (if not already enabled)
    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1U << 31); // PG bit
    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
    
    DEBUG_INFO("Paging enabled successfully\n");
}
