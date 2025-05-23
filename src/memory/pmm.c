#include "pmm.h"
#include "bitmap_mem.h"
#include "memory.h"
#include "graphic.h"  // Include for kprintf

// Global memory manager instance
static bitmap_memory_manager_t phys_mem;
static uint8_t bitmap_storage[BITMAP_MAX_BLOCKS / 8];  // Storage for the bitmap itself

// Track memory statistics
static size_t total_memory = 0;
static size_t reserved_memory = 0;
static size_t used_memory = 0;

bool physical_memory_init(struct limine_memmap_response *memmap) {
    if (!memmap) {
        return false;
    }
    
    // Find the largest usable memory region for our initial bitmap
    struct limine_memmap_entry *largest_region = NULL;
    size_t largest_size = 0;
    
    // Calculate total memory and find the largest usable region
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        
        // Add to total memory count for all usable memory
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_memory += entry->length;
            
            // Track the largest region
            if (entry->length > largest_size) {
                largest_size = entry->length;
                largest_region = entry;
            }
        } else {
            // Count reserved memory
            reserved_memory += entry->length;
        }
    }
    
    // If no usable memory found, fail
    if (!largest_region) {
        return false;
    }
    
    // Initialize the bitmap with the largest usable memory region
    if (!bitmap_init(&phys_mem, bitmap_storage, 
                    largest_region->base, largest_size)) {
        return false;
    }
    
    // Mark all non-usable or separate memory regions as reserved
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        
        // Skip our main memory region, it's already properly initialized
        if (entry == largest_region) {
            continue;
        }
        
        // Skip other usable regions for now - we'll add them later if needed
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            continue;
        }
        
        // If this region overlaps with our managed area, mark it as reserved
        if (bitmap_contains_address(&phys_mem, (void*)entry->base) ||
            bitmap_contains_address(&phys_mem, (void*)(entry->base + entry->length - 1))) {
            
            physical_reserve_region(entry->base, entry->length);
        }
    }
    
    // Reserve the bitmap storage itself to prevent it from being allocated
    physical_reserve_region((uintptr_t)bitmap_storage, sizeof(bitmap_storage));
    
    return true;
}

void *physical_alloc_page(void) {
    void *page = bitmap_alloc_block(&phys_mem);
    if (page) {
        used_memory += BITMAP_BLOCK_SIZE;
    }
    return page;
}

void *physical_alloc_pages(size_t count) {
    void *pages = bitmap_alloc_blocks(&phys_mem, count);
    if (pages) {
        used_memory += count * BITMAP_BLOCK_SIZE;
    }
    return pages;
}

void physical_free_page(void *page) {
    if (bitmap_contains_address(&phys_mem, page)) {
        bitmap_free_block(&phys_mem, page);
        if (used_memory >= BITMAP_BLOCK_SIZE) {
            used_memory -= BITMAP_BLOCK_SIZE;
        }
    }
}

void physical_free_pages(void *pages, size_t count) {
    if (bitmap_contains_address(&phys_mem, pages)) {
        bitmap_free_blocks(&phys_mem, pages, count);
        if (used_memory >= count * BITMAP_BLOCK_SIZE) {
            used_memory -= count * BITMAP_BLOCK_SIZE;
        }
    }
}

bool physical_reserve_region(uintptr_t base, size_t size) {
    // Calculate the start and end blocks
    size_t start_block = bitmap_address_to_block(&phys_mem, (void*)base);
    size_t end_address = base + size - 1;
    size_t end_block = bitmap_address_to_block(&phys_mem, (void*)end_address);
    
    // Make sure we don't exceed the bitmap bounds
    if (start_block >= phys_mem.total_blocks) {
        return false;
    }
    if (end_block >= phys_mem.total_blocks) {
        end_block = phys_mem.total_blocks - 1;
    }
    
    // Mark blocks as used
    for (size_t block = start_block; block <= end_block; block++) {
        void *addr = bitmap_block_to_address(&phys_mem, block);
        bitmap_free_block(&phys_mem, addr); // Ensure it's free first to avoid double counting
        bitmap_alloc_block(&phys_mem);      // Then mark it as used
    }
    
    return true;
}

size_t physical_get_total_memory(void) {
    return total_memory;
}

size_t physical_get_used_memory(void) {
    return used_memory + reserved_memory;
}

size_t physical_get_free_memory(void) {
    return bitmap_get_free_blocks(&phys_mem) * BITMAP_BLOCK_SIZE;
}

void physical_print_stats(size_t x, size_t y) {
    size_t total_kb = total_memory / 1024;
    size_t used_kb = (used_memory + reserved_memory) / 1024;
    size_t free_kb = physical_get_free_memory() / 1024;
    
    kprintf(x, y, "Memory Stats:");
    kprintf(x, y+=15, "Total: %d KB (%d MB)", total_kb, total_kb / 1024);
    kprintf(x, y+=15, "Used: %d KB (%d MB)", used_kb, used_kb / 1024);
    kprintf(x, y+=15, "Free: %d KB (%d MB)", free_kb, free_kb / 1024);
    kprintf(x, y+=15, "Managed blocks: %d", phys_mem.total_blocks);
    kprintf(x, y+=15, "Free blocks: %d", bitmap_get_free_blocks(&phys_mem));
}

void draw_memory_bitmap(size_t x, size_t y, size_t width, size_t height) {
    if (!phys_mem.bitmap) return;
    
    size_t bitmap_size = phys_mem.total_blocks;
    size_t pixels_per_bit = (width * height) / bitmap_size;
    if (pixels_per_bit < 1) pixels_per_bit = 1;
    
    size_t bits_per_row = width / pixels_per_bit;
    if (bits_per_row == 0) bits_per_row = 1;
    
    size_t rows = (bitmap_size + bits_per_row - 1) / bits_per_row;
    if (rows > height / pixels_per_bit) rows = height / pixels_per_bit;
    
    for (size_t i = 0; i < bitmap_size && i < bits_per_row * rows; i++) {
        size_t row = i / bits_per_row;
        size_t col = i % bits_per_row;
        
        // Calculate pixel position
        size_t px = x + col * pixels_per_bit;
        size_t py = y + row * pixels_per_bit;
        
        // Check if the bit is set (page is used)
        bool is_used = bitmap_test_bit(phys_mem.bitmap, i);
        
        // Choose color: green for free, red for used
        uint32_t color = is_used ? 0xFF0000 : 0x00FF00;
        
        // Draw a small rectangle for each bit
        draw_rect(px, py, pixels_per_bit, pixels_per_bit, 1, color, true);
    }
    
    // Draw a border around the bitmap
    draw_rect(x - 1, y - 1, width + 2, height + 2, 1, 0xFFFFFF, false);
    
    // Draw a legend
    draw_rect(x, y + height + 5, 10, 10, 1, 0x00FF00, true);
    draw_string(x + 15, y + height + 5, "Free", 0xFFFFFF);
    
    draw_rect(x, y + height + 20, 10, 10, 1, 0xFF0000, true);
    draw_string(x + 15, y + height + 20, "Used", 0xFFFFFF);
}