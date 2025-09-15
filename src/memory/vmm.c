#include "vmm.h"
#include "pmm.h"
#include "memory.h"
#include "../debug/debug.h"

// Global VMM instance
static vmm_t g_vmm = {0};

// Static page tables for initial setup (before dynamic allocation)
// This avoids the chicken-and-egg problem of needing memory allocation before VMM is ready
static page_table_t static_pml4 __attribute__((aligned(PAGE_SIZE)));
static page_table_t static_pdp[4] __attribute__((aligned(PAGE_SIZE)));  // Multiple PDP tables for different ranges
static page_table_t static_pd[4] __attribute__((aligned(PAGE_SIZE)));   // Multiple PD tables
static page_table_t static_pt[8] __attribute__((aligned(PAGE_SIZE)));   // Multiple PT tables

// External assembly function to enable paging and load CR3
extern void vmm_load_page_directory(uintptr_t pml4_physical);

// Internal helper functions
static page_table_t *vmm_get_or_create_table(page_table_t *parent, size_t index, uint64_t flags);
static void vmm_invalidate_page(uintptr_t virtual_addr);

bool vmm_init(void) {
    DEBUG_INFO("Initializing Virtual Memory Manager");
    
    DEBUG_INFO("Using static PML4 table for initial setup");
    // Use static memory for initial page tables to avoid chicken-and-egg problem
    g_vmm.pml4 = &static_pml4;
    
    DEBUG_INFO("About to clear PML4 table");
    // Clear the PML4 table
    memset(g_vmm.pml4, 0, sizeof(page_table_t));
    DEBUG_INFO("PML4 table cleared");
    
    // Store the physical address of PML4 for CR3 loading
    // Since paging is not enabled yet, the virtual address IS the physical address
    g_vmm.kernel_pml4_phys = (uintptr_t)g_vmm.pml4;
    g_vmm.paging_enabled = false;
    
    DEBUG_INFO("VMM initialized with PML4 at physical address: %p", (void*)g_vmm.kernel_pml4_phys);
    return true;
}

vmm_t *vmm_get_current(void) {
    return &g_vmm;
}

bool vmm_map_page(uintptr_t virtual_addr, uintptr_t physical_addr, uint64_t flags) {
    if (!vmm_is_page_aligned(virtual_addr) || !vmm_is_page_aligned(physical_addr)) {
        DEBUG_ERROR("Addresses must be page-aligned");
        return false;
    }
    
    DEBUG_INFO("Mapping virtual %p to physical %p", (void*)virtual_addr, (void*)physical_addr);
    
    // Extract page table indices
    size_t pml4_idx = PML4_INDEX(virtual_addr);
    size_t pdp_idx = PDP_INDEX(virtual_addr);
    size_t pd_idx = PD_INDEX(virtual_addr);
    size_t pt_idx = PT_INDEX(virtual_addr);
    
    DEBUG_INFO("Indices: PML4=%d, PDP=%d, PD=%d, PT=%d", (int)pml4_idx, (int)pdp_idx, (int)pd_idx, (int)pt_idx);
    
    DEBUG_INFO("Getting/creating PDP table");
    // Get or create Page Directory Pointer table
    page_table_t *pdp = vmm_get_or_create_table(g_vmm.pml4, pml4_idx, 
                                                PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER));
    if (!pdp) {
        DEBUG_ERROR("Failed to get/create PDP table");
        return false;
    }
    DEBUG_INFO("PDP table at: %p", pdp);
    
    DEBUG_INFO("Getting/creating PD table");
    // Get or create Page Directory table
    page_table_t *pd = vmm_get_or_create_table(pdp, pdp_idx, 
                                               PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER));
    if (!pd) {
        DEBUG_ERROR("Failed to get/create PD table");
        return false;
    }
    DEBUG_INFO("PD table at: %p", pd);
    
    DEBUG_INFO("Getting/creating PT table");
    // Get or create Page Table
    page_table_t *pt = vmm_get_or_create_table(pd, pd_idx, 
                                               PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER));
    if (!pt) {
        DEBUG_ERROR("Failed to get/create PT table");
        return false;
    }
    DEBUG_INFO("PT table at: %p", pt);
    
    DEBUG_INFO("Setting page table entry");
    // Map the page
    pt->entries[pt_idx] = physical_addr | flags | PTE_PRESENT;
    
    DEBUG_INFO("Invalidating TLB entry");
    // Invalidate the TLB entry for this virtual address
    vmm_invalidate_page(virtual_addr);
    
    DEBUG_INFO("Page mapping completed successfully");
    return true;
}

bool vmm_unmap_page(uintptr_t virtual_addr) {
    if (!vmm_is_page_aligned(virtual_addr)) {
        DEBUG_ERROR("Virtual address must be page-aligned");
        return false;
    }
    
    // Extract page table indices
    size_t pml4_idx = PML4_INDEX(virtual_addr);
    size_t pdp_idx = PDP_INDEX(virtual_addr);
    size_t pd_idx = PD_INDEX(virtual_addr);
    size_t pt_idx = PT_INDEX(virtual_addr);
    
    // Check if PML4 entry exists
    if (!(g_vmm.pml4->entries[pml4_idx] & PTE_PRESENT)) {
        return false; // Not mapped
    }
    
    page_table_t *pdp = (page_table_t *)PTE_GET_ADDR(g_vmm.pml4->entries[pml4_idx]);
    if (!(pdp->entries[pdp_idx] & PTE_PRESENT)) {
        return false; // Not mapped
    }
    
    page_table_t *pd = (page_table_t *)PTE_GET_ADDR(pdp->entries[pdp_idx]);
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return false; // Not mapped
    }
    
    page_table_t *pt = (page_table_t *)PTE_GET_ADDR(pd->entries[pd_idx]);
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
        return false; // Not mapped
    }
    
    // Unmap the page
    pt->entries[pt_idx] = 0;
    
    // Invalidate the TLB entry
    vmm_invalidate_page(virtual_addr);
    
    return true;
}

bool vmm_map_pages(uintptr_t virtual_addr, uintptr_t physical_addr, size_t count, uint64_t flags) {
    for (size_t i = 0; i < count; i++) {
        if (!vmm_map_page(virtual_addr + (i * PAGE_SIZE), 
                         physical_addr + (i * PAGE_SIZE), flags)) {
            // Cleanup on failure
            for (size_t j = 0; j < i; j++) {
                vmm_unmap_page(virtual_addr + (j * PAGE_SIZE));
            }
            return false;
        }
    }
    return true;
}

bool vmm_unmap_pages(uintptr_t virtual_addr, size_t count) {
    bool success = true;
    for (size_t i = 0; i < count; i++) {
        if (!vmm_unmap_page(virtual_addr + (i * PAGE_SIZE))) {
            success = false;
        }
    }
    return success;
}

uintptr_t vmm_get_physical_address(uintptr_t virtual_addr) {
    // If paging is not enabled, assume identity mapping
    if (!g_vmm.paging_enabled) {
        return virtual_addr;
    }
    
    // Extract page table indices
    size_t pml4_idx = PML4_INDEX(virtual_addr);
    size_t pdp_idx = PDP_INDEX(virtual_addr);
    size_t pd_idx = PD_INDEX(virtual_addr);
    size_t pt_idx = PT_INDEX(virtual_addr);
    size_t page_offset = virtual_addr & PAGE_ALIGN_MASK;
    
    // Walk the page tables
    if (!(g_vmm.pml4->entries[pml4_idx] & PTE_PRESENT)) {
        return 0; // Not mapped
    }
    
    page_table_t *pdp = (page_table_t *)PTE_GET_ADDR(g_vmm.pml4->entries[pml4_idx]);
    if (!(pdp->entries[pdp_idx] & PTE_PRESENT)) {
        return 0; // Not mapped
    }
    
    page_table_t *pd = (page_table_t *)PTE_GET_ADDR(pdp->entries[pdp_idx]);
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return 0; // Not mapped
    }
    
    page_table_t *pt = (page_table_t *)PTE_GET_ADDR(pd->entries[pd_idx]);
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
        return 0; // Not mapped
    }
    
    return PTE_GET_ADDR(pt->entries[pt_idx]) + page_offset;
}

bool vmm_is_mapped(uintptr_t virtual_addr) {
    return vmm_get_physical_address(virtual_addr) != 0;
}

bool vmm_identity_map_range(uintptr_t start, uintptr_t end, uint64_t flags) {
    start = vmm_page_align_down(start);
    end = vmm_page_align_up(end);
    
    DEBUG_INFO("Identity mapping range: %p - %p", (void*)start, (void*)end);
    DEBUG_INFO("Will map %d pages", (int)((end - start) / PAGE_SIZE));
    
    for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
        DEBUG_INFO("Mapping page at %p", (void*)addr);
        if (!vmm_map_page(addr, addr, flags)) {
            DEBUG_ERROR("Failed to identity map page at %p", (void*)addr);
            return false;
        }
        DEBUG_INFO("Successfully mapped page at %p", (void*)addr);
    }
    
    DEBUG_INFO("Completed mapping range %p - %p", (void*)start, (void*)end);
    return true;
}

bool vmm_enable_paging(void) {
    if (g_vmm.paging_enabled) {
        return true; // Already enabled
    }
    
    DEBUG_INFO("About to enable paging with PML4 at physical %p", (void*)g_vmm.kernel_pml4_phys);
    DEBUG_INFO("Current CR0 before paging enable");
    
    // Load the page directory and enable paging
    DEBUG_INFO("Calling vmm_load_page_directory...");
    vmm_load_page_directory(g_vmm.kernel_pml4_phys);
    DEBUG_INFO("vmm_load_page_directory returned successfully");
    
    g_vmm.paging_enabled = true;
    
    DEBUG_INFO("Paging enabled successfully - virtual memory is now active");
    return true;
}

void *vmm_alloc_kernel_pages(size_t count) {
    // Allocate physical pages
    void *physical_pages = physical_alloc_pages(count);
    if (!physical_pages) {
        return NULL;
    }
    
    // For now, return the physical address directly (identity mapped)
    // TODO: Implement proper kernel heap virtual addressing
    return physical_pages;
}

void vmm_free_kernel_pages(void *virtual_addr, size_t count) {
    // For now, free the physical pages directly
    // TODO: Implement proper virtual address translation
    physical_free_pages(virtual_addr, count);
}

// Internal helper functions

// Track which static tables we've used
static size_t static_pdp_used = 0;
static size_t static_pd_used = 0;
static size_t static_pt_used = 0;

static page_table_t *vmm_get_or_create_table(page_table_t *parent, size_t index, uint64_t flags) {
    DEBUG_INFO("Checking if entry %zu exists in parent table %p", index, parent);
    DEBUG_INFO("Parent entry[%zu] = 0x%lx", index, parent->entries[index]);
    
    if (parent->entries[index] & PTE_PRESENT) {
        // Table already exists, return it
        uint64_t entry_raw = parent->entries[index];
        uintptr_t addr = PTE_GET_ADDR(entry_raw);
        page_table_t *existing = (page_table_t *)addr;
        DEBUG_INFO("Table already exists: entry=0x%lx, addr=0x%lx, ptr=%p", entry_raw, addr, existing);
        return existing;
    }
    
    DEBUG_INFO("Entry not present, need to create new table");
    
    page_table_t *new_table = NULL;
    
    // Use static tables for initial setup to avoid allocation issues
    if (parent == &static_pml4 && static_pdp_used < 4) {
        new_table = &static_pdp[static_pdp_used];
        static_pdp_used++;
        DEBUG_INFO("Using static PDP table");
    } else if ((parent >= &static_pdp[0] && parent <= &static_pdp[3]) && static_pd_used < 4) {
        new_table = &static_pd[static_pd_used];
        static_pd_used++;
        DEBUG_INFO("Using static PD table");
    } else if ((parent >= &static_pd[0] && parent <= &static_pd[3]) && static_pt_used < 8) {
        new_table = &static_pt[static_pt_used];
        static_pt_used++;
        DEBUG_INFO("Using static PT table");
    } else {
        // Fallback to dynamic allocation (for later use)
        DEBUG_INFO("Attempting dynamic allocation for page table");
        new_table = (page_table_t *)physical_alloc_page();
        if (!new_table) {
            DEBUG_ERROR("Failed to allocate page table dynamically");
            return NULL;
        }
    }
    
    DEBUG_INFO("Allocated new page table at %p", new_table);
    
    // Clear the new table
    memset(new_table, 0, sizeof(page_table_t));
    DEBUG_INFO("Cleared new page table");
    
    // Add entry to parent table
    // For now, assume identity mapping since we're setting up initial mappings
    uintptr_t physical_addr = (uintptr_t)new_table;
    parent->entries[index] = physical_addr | flags | PTE_PRESENT;
    DEBUG_INFO("Added entry to parent table: entry[%d] = %p", (int)index, (void*)(parent->entries[index]));
    
    return new_table;
}

static void vmm_invalidate_page(uintptr_t virtual_addr) {
    // Invalidate TLB entry for this virtual address
    asm volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

bool vmm_setup_initial_mappings(void) {
    DEBUG_INFO("Setting up initial identity mappings");
    
    // Start with just the first 16KB for low memory
    DEBUG_INFO("Mapping first 16KB (4 pages) for low memory");
    if (!vmm_identity_map_range(0x0, 0x4000, PTE_PRESENT | PTE_WRITABLE)) {
        DEBUG_ERROR("Failed to identity map first 16KB");
        return false;
    }
    DEBUG_INFO("Successfully mapped first 16KB");
    
    DEBUG_INFO("Initial identity mappings completed successfully");
    return true;
}

bool vmm_setup_initial_mappings_with_kernel(uintptr_t kernel_virt_base, uintptr_t kernel_phys_base) {
    DEBUG_INFO("Setting up initial identity mappings with kernel mapping");
    
    // Start with just the first 16KB for low memory
    DEBUG_INFO("Mapping first 16KB (4 pages) for low memory");
    if (!vmm_identity_map_range(0x0, 0x4000, PTE_PRESENT | PTE_WRITABLE)) {
        DEBUG_ERROR("Failed to identity map first 16KB");
        return false;
    }
    DEBUG_INFO("Successfully mapped first 16KB");
    
    // Map kernel region (2MB should be enough for kernel code and data)
    DEBUG_INFO("Mapping kernel from virtual %p to physical %p", (void*)kernel_virt_base, (void*)kernel_phys_base);
    size_t kernel_size = 2 * 1024 * 1024; // 2MB
    
    for (uintptr_t offset = 0; offset < kernel_size; offset += PAGE_SIZE) {
        uintptr_t virt = kernel_virt_base + offset;
        uintptr_t phys = kernel_phys_base + offset;
        
        if (!vmm_map_page(virt, phys, PTE_PRESENT | PTE_WRITABLE)) {
            DEBUG_ERROR("Failed to map kernel page at virtual %p to physical %p", (void*)virt, (void*)phys);
            return false;
        }
    }
    DEBUG_INFO("Successfully mapped kernel region");
    
    DEBUG_INFO("Initial identity mappings with kernel completed successfully");
    return true;
}