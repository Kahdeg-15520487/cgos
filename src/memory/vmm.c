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

// HHDM (Higher Half Direct Map) offset from Limine
static uint64_t hhdm_offset = 0;

// HHDM getter and setter
uint64_t vmm_get_hhdm_offset(void) {
    return hhdm_offset;
}

void vmm_set_hhdm_offset(uint64_t offset) {
    hhdm_offset = offset;
    DEBUG_INFO("HHDM offset set to: 0x%lx\n", offset);
}

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
    DEBUG_DEBUG("vmm_map_page: phys=0x%lx virt=0x%lx flags=0x%lx\n", 
               physical_addr, virtual_addr, flags);
    
    if (hhdm_offset == 0) {
        DEBUG_ERROR("HHDM not initialized! Cannot map page.\n");
        return NULL;
    }
    
    // Get PML4 via HHDM (convert physical CR3 to virtual)
    uint64_t cr3_phys = kernel_context.cr3_value & ~0xFFFULL;
    page_table_t *pml4 = (page_table_t*)(cr3_phys + hhdm_offset);
    
    // Get page table indices
    int pml4_idx = PML4_INDEX(virtual_addr);
    int pdp_idx = PDP_INDEX(virtual_addr);
    int pd_idx = PD_INDEX(virtual_addr);
    int pt_idx = PT_INDEX(virtual_addr);
    
    // Walk/create PDP
    page_table_t *pdp;
    if (pml4->entries[pml4_idx] & PAGE_PRESENT) {
        uint64_t pdp_phys = pml4->entries[pml4_idx] & ~PAGE_MASK;
        pdp = (page_table_t*)(pdp_phys + hhdm_offset);
    } else {
        void *new_page = physical_alloc_page();
        if (!new_page) {
            DEBUG_ERROR("Failed to allocate PDP\n");
            return NULL;
        }
        memset((void*)((uint64_t)new_page + hhdm_offset), 0, PAGE_SIZE);
        pml4->entries[pml4_idx] = (uint64_t)new_page | PAGE_PRESENT | PAGE_WRITABLE;
        pdp = (page_table_t*)((uint64_t)new_page + hhdm_offset);
    }
    
    // Walk/create PD
    page_table_t *pd;
    if (pdp->entries[pdp_idx] & PAGE_PRESENT) {
        uint64_t pd_phys = pdp->entries[pdp_idx] & ~PAGE_MASK;
        pd = (page_table_t*)(pd_phys + hhdm_offset);
    } else {
        void *new_page = physical_alloc_page();
        if (!new_page) {
            DEBUG_ERROR("Failed to allocate PD\n");
            return NULL;
        }
        memset((void*)((uint64_t)new_page + hhdm_offset), 0, PAGE_SIZE);
        pdp->entries[pdp_idx] = (uint64_t)new_page | PAGE_PRESENT | PAGE_WRITABLE;
        pd = (page_table_t*)((uint64_t)new_page + hhdm_offset);
    }
    
    // Walk/create PT
    page_table_t *pt;
    if (pd->entries[pd_idx] & PAGE_PRESENT) {
        uint64_t pt_phys = pd->entries[pd_idx] & ~PAGE_MASK;
        pt = (page_table_t*)(pt_phys + hhdm_offset);
    } else {
        void *new_page = physical_alloc_page();
        if (!new_page) {
            DEBUG_ERROR("Failed to allocate PT\n");
            return NULL;
        }
        memset((void*)((uint64_t)new_page + hhdm_offset), 0, PAGE_SIZE);
        pd->entries[pd_idx] = (uint64_t)new_page | PAGE_PRESENT | PAGE_WRITABLE;
        pt = (page_table_t*)((uint64_t)new_page + hhdm_offset);
    }
    
    // Map the actual page with provided flags
    uint64_t page_flags = PAGE_PRESENT | (flags & (PAGE_WRITABLE | PAGE_USER | PAGE_PCD | PAGE_PWT));
    pt->entries[pt_idx] = physical_addr | page_flags;
    
    // Flush TLB for this page
    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
    
    DEBUG_DEBUG("vmm_map_page: Mapped phys=0x%lx -> virt=0x%lx\n", physical_addr, virtual_addr);
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
    
    if (hhdm_offset == 0) {
        DEBUG_ERROR("HHDM not initialized! Cannot map MMIO.\n");
        return NULL;
    }
    
    // Allocate virtual address space for MMIO
    uint64_t vaddr = next_mmio_vaddr;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    next_mmio_vaddr += pages * PAGE_SIZE;
    
    DEBUG_INFO("Allocating MMIO virtual range: 0x%lx - 0x%lx (%zu pages)\n", 
               vaddr, next_mmio_vaddr, pages);
    
    // Get PML4 via HHDM (convert physical CR3 to virtual)
    uint64_t cr3_phys = kernel_context.cr3_value & ~0xFFFULL;
    page_table_t *pml4 = (page_table_t*)(cr3_phys + hhdm_offset);
    
    DEBUG_DEBUG("PML4 at physical 0x%lx, virtual 0x%lx\n", cr3_phys, (uint64_t)pml4);
    
    // Map each page
    for (size_t i = 0; i < pages; i++) {
        uint64_t paddr = physical_addr + (i * PAGE_SIZE);
        uint64_t this_vaddr = vaddr + (i * PAGE_SIZE);
        
        // Get page table indices
        int pml4_idx = PML4_INDEX(this_vaddr);
        int pdp_idx = PDP_INDEX(this_vaddr);
        int pd_idx = PD_INDEX(this_vaddr);
        int pt_idx = PT_INDEX(this_vaddr);
        
        DEBUG_DEBUG("Mapping page: virt=0x%lx -> phys=0x%lx\n", this_vaddr, paddr);
        DEBUG_DEBUG("  Indices: PML4=%d, PDP=%d, PD=%d, PT=%d\n", 
                   pml4_idx, pdp_idx, pd_idx, pt_idx);
        
        // Walk/create PDP
        page_table_t *pdp;
        if (pml4->entries[pml4_idx] & PAGE_PRESENT) {
            uint64_t pdp_phys = pml4->entries[pml4_idx] & ~PAGE_MASK;
            pdp = (page_table_t*)(pdp_phys + hhdm_offset);
        } else {
            void *new_page = physical_alloc_page();
            if (!new_page) {
                DEBUG_ERROR("Failed to allocate PDP\n");
                return NULL;
            }
            memset((void*)((uint64_t)new_page + hhdm_offset), 0, PAGE_SIZE);
            pml4->entries[pml4_idx] = (uint64_t)new_page | PAGE_PRESENT | PAGE_WRITABLE;
            pdp = (page_table_t*)((uint64_t)new_page + hhdm_offset);
            DEBUG_DEBUG("  Created new PDP at phys 0x%lx\n", (uint64_t)new_page);
        }
        
        // Walk/create PD
        page_table_t *pd;
        if (pdp->entries[pdp_idx] & PAGE_PRESENT) {
            uint64_t pd_phys = pdp->entries[pdp_idx] & ~PAGE_MASK;
            pd = (page_table_t*)(pd_phys + hhdm_offset);
        } else {
            void *new_page = physical_alloc_page();
            if (!new_page) {
                DEBUG_ERROR("Failed to allocate PD\n");
                return NULL;
            }
            memset((void*)((uint64_t)new_page + hhdm_offset), 0, PAGE_SIZE);
            pdp->entries[pdp_idx] = (uint64_t)new_page | PAGE_PRESENT | PAGE_WRITABLE;
            pd = (page_table_t*)((uint64_t)new_page + hhdm_offset);
            DEBUG_DEBUG("  Created new PD at phys 0x%lx\n", (uint64_t)new_page);
        }
        
        // Walk/create PT
        page_table_t *pt;
        if (pd->entries[pd_idx] & PAGE_PRESENT) {
            uint64_t pt_phys = pd->entries[pd_idx] & ~PAGE_MASK;
            pt = (page_table_t*)(pt_phys + hhdm_offset);
        } else {
            void *new_page = physical_alloc_page();
            if (!new_page) {
                DEBUG_ERROR("Failed to allocate PT\n");
                return NULL;
            }
            memset((void*)((uint64_t)new_page + hhdm_offset), 0, PAGE_SIZE);
            pd->entries[pd_idx] = (uint64_t)new_page | PAGE_PRESENT | PAGE_WRITABLE;
            pt = (page_table_t*)((uint64_t)new_page + hhdm_offset);
            DEBUG_DEBUG("  Created new PT at phys 0x%lx\n", (uint64_t)new_page);
        }
        
        // Map the actual page with cache-disable for MMIO
        uint64_t page_flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_PCD | PAGE_PWT;
        pt->entries[pt_idx] = paddr | page_flags;
        
        DEBUG_DEBUG("  PT[%d] = 0x%lx (flags: PCD+PWT for uncached MMIO)\n", 
                   pt_idx, pt->entries[pt_idx]);
    }
    
    // Flush TLB for the new mappings
    for (size_t i = 0; i < pages; i++) {
        uint64_t flush_addr = vaddr + (i * PAGE_SIZE);
        asm volatile("invlpg (%0)" :: "r"(flush_addr) : "memory");
    }
    
    DEBUG_INFO("MMIO mapped successfully: phys=0x%lx -> virt=0x%lx\n", physical_addr, vaddr);
    return (void*)vaddr;
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
