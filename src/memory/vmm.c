#include "vmm.h"
#include "pmm.h"
#include "memory.h"
#include "../debug/debug.h"

// Global VMM instance
static vmm_t g_vmm = {0};

// Static page tables for initial setup (before dynamic allocation)
// This avoids the chicken-and-egg problem of needing memory allocation before VMM is ready
static page_table_t static_pml4 __attribute__((aligned(PAGE_SIZE)));
static page_table_t static_pdp[8] __attribute__((aligned(PAGE_SIZE)));  // More PDP tables for better coverage
static page_table_t static_pd[16] __attribute__((aligned(PAGE_SIZE)));  // More PD tables for identity mapping
static page_table_t static_pt[8] __attribute__((aligned(PAGE_SIZE)));   // PT tables for detailed mapping

// Tracking for static table usage
static size_t static_pdp_used = 0;
static size_t static_pd_used = 0;
static size_t static_pt_used = 0;

// Virtual address space management for kernel heap
#define KERNEL_HEAP_SIZE (64 * 1024 * 1024)  // 64MB kernel heap
#define MAX_HEAP_BLOCKS 256

typedef struct {
    uintptr_t virtual_addr;
    size_t size;
    bool is_free;
} heap_block_t;

static heap_block_t kernel_heap_blocks[MAX_HEAP_BLOCKS];
static bool heap_initialized = false;

// External assembly function to enable paging and load CR3
extern void vmm_load_page_directory(uintptr_t pml4_physical);

// Internal helper functions
static page_table_t *vmm_get_or_create_table(page_table_t *parent, size_t index, uint64_t flags);
static void vmm_invalidate_page(uintptr_t virtual_addr);
static bool vmm_init_kernel_heap(void);
static uintptr_t vmm_alloc_virtual_pages(size_t count);
static void vmm_free_virtual_pages(uintptr_t virtual_addr, size_t count);

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
    
    // Set up initial identity mappings for low memory (needed for kernel operation)
    if (!vmm_setup_initial_mappings()) {
        DEBUG_ERROR("Failed to set up initial identity mappings");
        return false;
    }
    
    // TODO: Temporarily disable paging enable to debug crash
    DEBUG_INFO("Skipping paging enable for debugging - page tables set up but paging not enabled");
    /*
    // Enable paging to activate virtual memory
    if (!vmm_enable_paging()) {
        DEBUG_ERROR("Failed to enable paging");
        return false;
    }
    */
    
    // Initialize kernel heap virtual address management
    if (!vmm_init_kernel_heap()) {
        DEBUG_ERROR("Failed to initialize kernel heap");
        return false;
    }
    
    DEBUG_INFO("VMM fully initialized with paging enabled at PML4 %p", (void*)g_vmm.kernel_pml4_phys);
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
    DEBUG_INFO("Allocating %zu kernel pages", count);
    
    // If paging is not enabled, fall back to simple physical allocation
    if (!g_vmm.paging_enabled) {
        DEBUG_INFO("Paging not enabled - using physical allocation fallback");
        void *physical_pages = physical_alloc_pages(count);
        if (physical_pages) {
            DEBUG_INFO("Allocated %zu pages at physical %p", count, physical_pages);
        }
        return physical_pages;
    }
    
    // Step 1: Allocate virtual address space
    uintptr_t virtual_addr = vmm_alloc_virtual_pages(count);
    if (!virtual_addr) {
        DEBUG_ERROR("Failed to allocate virtual address space");
        return NULL;
    }
    
    // Step 2: Allocate physical pages
    void *physical_pages = physical_alloc_pages(count);
    if (!physical_pages) {
        DEBUG_ERROR("Failed to allocate physical pages");
        vmm_free_virtual_pages(virtual_addr, count);
        return NULL;
    }
    
    // Step 3: Map virtual pages to physical pages
    uintptr_t phys_addr = (uintptr_t)physical_pages;
    for (size_t i = 0; i < count; i++) {
        uintptr_t virt_page = virtual_addr + (i * PAGE_SIZE);
        uintptr_t phys_page = phys_addr + (i * PAGE_SIZE);
        
        if (!vmm_map_page(virt_page, phys_page, PTE_PRESENT | PTE_WRITABLE)) {
            DEBUG_ERROR("Failed to map virtual page %p to physical page %p", 
                       (void*)virt_page, (void*)phys_page);
            
            // Clean up any mappings we've already created
            for (size_t j = 0; j < i; j++) {
                vmm_unmap_page(virtual_addr + (j * PAGE_SIZE));
            }
            physical_free_pages(physical_pages, count);
            vmm_free_virtual_pages(virtual_addr, count);
            return NULL;
        }
    }
    
    DEBUG_INFO("Successfully allocated %zu kernel pages at virtual %p (physical %p)", 
               count, (void*)virtual_addr, physical_pages);
    return (void*)virtual_addr;
}

void vmm_free_kernel_pages(void *virtual_addr, size_t count) {
    if (!virtual_addr) {
        return;
    }
    
    // If paging is not enabled, fall back to simple physical free
    if (!g_vmm.paging_enabled) {
        DEBUG_INFO("Paging not enabled - using physical free fallback");
        physical_free_pages(virtual_addr, count);
        return;
    }
    
    uintptr_t virt_addr = (uintptr_t)virtual_addr;
    DEBUG_INFO("Freeing %zu kernel pages at virtual %p", count, virtual_addr);
    
    // Step 1: Get physical addresses and unmap virtual pages
    uintptr_t first_phys_addr = 0;
    for (size_t i = 0; i < count; i++) {
        uintptr_t virt_page = virt_addr + (i * PAGE_SIZE);
        uintptr_t phys_page = vmm_get_physical_address(virt_page);
        
        if (i == 0) {
            first_phys_addr = phys_page;
        }
        
        if (!vmm_unmap_page(virt_page)) {
            DEBUG_ERROR("Failed to unmap virtual page %p", (void*)virt_page);
        }
    }
    
    // Step 2: Free physical pages
    if (first_phys_addr) {
        physical_free_pages((void*)first_phys_addr, count);
    }
    
    // Step 3: Free virtual address space
    vmm_free_virtual_pages(virt_addr, count);
    
    DEBUG_INFO("Successfully freed %zu kernel pages", count);
}

// Internal helper functions

static page_table_t *vmm_get_or_create_table(page_table_t *parent, size_t index, uint64_t flags) {
    // Add safety checks
    if (!parent) {
        DEBUG_ERROR("Parent table is NULL!");
        return NULL;
    }
    if (index >= 512) {
        DEBUG_ERROR("Index %zu is out of range (max 511)", index);
        return NULL;
    }
    
    DEBUG_INFO("Checking if entry %zu exists in parent table %p", index, parent);
    
    // Debug: Print static table addresses for reference
    DEBUG_INFO("Static table addresses: PML4=%p, PDP=%p, PD=%p, PT=%p", 
               &static_pml4, &static_pdp[0], &static_pd[0], &static_pt[0]);
    
    // Add basic null check
    if (!parent) {
        DEBUG_ERROR("Parent table is NULL!");
        return NULL;
    }
    
    DEBUG_INFO("About to read parent->entries[%zu] from parent table %p", index, parent);
    uint64_t parent_entry = parent->entries[index];
    DEBUG_INFO("Successfully read parent entry[%zu] = 0x%lx", index, parent_entry);
    
    if (parent_entry & PTE_PRESENT) {
        // Table already exists - but there's a memory corruption issue with reuse
        uintptr_t addr = PTE_GET_ADDR(parent_entry);
        DEBUG_INFO("Found existing table: entry=0x%lx, extracted addr=0x%lx", parent_entry, addr);
        
        // TEMPORARY: Only reuse PD and PT tables, not PDP tables
        // Check if this is a PDP table reuse (parent is PML4)
        if (parent == &static_pml4) {
            DEBUG_INFO("This is PDP table reuse - SKIPPING due to memory corruption issue");
            // Fall through to create new table instead
        } else {
            // This is PD or PT table reuse - should be safer
            page_table_t *existing = (page_table_t *)addr;
            DEBUG_INFO("Reusing non-PDP table at %p", existing);
            return existing;
        }
    }
    
    DEBUG_INFO("Entry not present, need to create new table");
    
    page_table_t *new_table = NULL;
    
    // Use static tables for initial setup to avoid allocation issues
    if (parent == &static_pml4 && static_pdp_used < 8) {
        new_table = &static_pdp[static_pdp_used];
        static_pdp_used++;
        DEBUG_INFO("Using static PDP table %zu/8", static_pdp_used);
    } else if ((parent >= &static_pdp[0] && parent <= &static_pdp[7]) && static_pd_used < 16) {
        new_table = &static_pd[static_pd_used];
        static_pd_used++;
        DEBUG_INFO("Using static PD table %zu/16", static_pd_used);
    } else if ((parent >= &static_pd[0] && parent <= &static_pd[15]) && static_pt_used < 8) {
        new_table = &static_pt[static_pt_used];
        static_pt_used++;
        DEBUG_INFO("Using static PT table %zu/8", static_pt_used);
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
    
    // Clear the new table - do it manually instead of using memset
    DEBUG_INFO("About to clear new page table at %p", new_table);
    for (int i = 0; i < 512; i++) {
        new_table->entries[i] = 0;
    }
    DEBUG_INFO("Manually cleared new page table");
    
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
    
    // Test with 2 pages to isolate the crash
    DEBUG_INFO("Mapping 2 pages (8KB) for debugging");
    if (!vmm_identity_map_range(0x0, 0x2000, PTE_PRESENT | PTE_WRITABLE)) {
        DEBUG_ERROR("Failed to identity map 2 pages");
        return false;
    }
    DEBUG_INFO("Successfully mapped 2 pages");
    
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

// Kernel heap management functions

static bool vmm_init_kernel_heap(void) {
    DEBUG_INFO("Initializing kernel heap virtual address space");
    
    // Initialize all heap blocks as free
    for (int i = 0; i < MAX_HEAP_BLOCKS; i++) {
        kernel_heap_blocks[i].virtual_addr = 0;
        kernel_heap_blocks[i].size = 0;
        kernel_heap_blocks[i].is_free = true;
    }
    
    // Create an initial large free block covering the entire heap space
    kernel_heap_blocks[0].virtual_addr = KERNEL_HEAP_BASE;
    kernel_heap_blocks[0].size = KERNEL_HEAP_SIZE;
    kernel_heap_blocks[0].is_free = true;
    
    heap_initialized = true;
    DEBUG_INFO("Kernel heap initialized: base=%p, size=%zu MB", 
               (void*)KERNEL_HEAP_BASE, KERNEL_HEAP_SIZE / (1024 * 1024));
    return true;
}

static uintptr_t vmm_alloc_virtual_pages(size_t count) {
    if (!heap_initialized) {
        DEBUG_ERROR("Kernel heap not initialized");
        return 0;
    }
    
    size_t needed_size = count * PAGE_SIZE;
    DEBUG_INFO("Looking for %zu virtual pages (%zu bytes)", count, needed_size);
    
    // Find a free block large enough
    for (int i = 0; i < MAX_HEAP_BLOCKS; i++) {
        if (kernel_heap_blocks[i].is_free && kernel_heap_blocks[i].size >= needed_size) {
            uintptr_t allocated_addr = kernel_heap_blocks[i].virtual_addr;
            
            // If block is exactly the right size, mark it as used
            if (kernel_heap_blocks[i].size == needed_size) {
                kernel_heap_blocks[i].is_free = false;
                DEBUG_INFO("Allocated virtual pages at %p (exact fit)", (void*)allocated_addr);
                return allocated_addr;
            }
            
            // Otherwise, split the block
            // Find a free slot for the remaining part
            for (int j = 0; j < MAX_HEAP_BLOCKS; j++) {
                if (kernel_heap_blocks[j].virtual_addr == 0) {  // Empty slot
                    // Set up the remaining free block
                    kernel_heap_blocks[j].virtual_addr = allocated_addr + needed_size;
                    kernel_heap_blocks[j].size = kernel_heap_blocks[i].size - needed_size;
                    kernel_heap_blocks[j].is_free = true;
                    
                    // Update the allocated block
                    kernel_heap_blocks[i].size = needed_size;
                    kernel_heap_blocks[i].is_free = false;
                    
                    DEBUG_INFO("Allocated virtual pages at %p (split block)", (void*)allocated_addr);
                    return allocated_addr;
                }
            }
            
            DEBUG_ERROR("No free heap block slots available for splitting");
            return 0;
        }
    }
    
    DEBUG_ERROR("No suitable free virtual address space found");
    return 0;
}

static void vmm_free_virtual_pages(uintptr_t virtual_addr, size_t count) {
    if (!heap_initialized) {
        DEBUG_ERROR("Kernel heap not initialized");
        return;
    }
    
    size_t size = count * PAGE_SIZE;
    DEBUG_INFO("Freeing %zu virtual pages at %p", count, (void*)virtual_addr);
    
    // Find the block to free
    for (int i = 0; i < MAX_HEAP_BLOCKS; i++) {
        if (kernel_heap_blocks[i].virtual_addr == virtual_addr && 
            kernel_heap_blocks[i].size == size && 
            !kernel_heap_blocks[i].is_free) {
            
            kernel_heap_blocks[i].is_free = true;
            DEBUG_INFO("Freed virtual pages at %p", (void*)virtual_addr);
            
            // TODO: Implement block coalescing to merge adjacent free blocks
            return;
        }
    }
    
    DEBUG_ERROR("Virtual address %p not found in allocation table", (void*)virtual_addr);
}