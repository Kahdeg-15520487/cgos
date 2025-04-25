#ifndef PHYSICAL_MEM_H
#define PHYSICAL_MEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

// Initialize the physical memory manager using the memory map from Limine
bool physical_memory_init(struct limine_memmap_response *memmap);

// Get information about physical memory
size_t physical_get_total_memory(void);
size_t physical_get_used_memory(void);
size_t physical_get_free_memory(void);

// Allocate a single page of physical memory
void *physical_alloc_page(void);

// Allocate multiple contiguous pages
void *physical_alloc_pages(size_t count);

// Free a previously allocated page
void physical_free_page(void *page);

// Free multiple pages
void physical_free_pages(void *pages, size_t count);

// Reserve a specific memory region (mark as unavailable)
bool physical_reserve_region(uintptr_t base, size_t size);

// Debug function to print memory statistics
void physical_print_stats(size_t x, size_t y);

// Add this function prototype to pmm.h
void draw_memory_bitmap(size_t x, size_t y, size_t width, size_t height);

#endif // PHYSICAL_MEM_H