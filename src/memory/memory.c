#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "memory.h"
#include "pmm.h"
#include "bitmap_mem.h"
#include "../graphic/graphic.h"

// Memory allocation tracking structure
typedef struct allocation_header {
    size_t size;                    // Size of the allocation
    uint32_t magic;                 // Magic number for validation
    struct allocation_header *next; // Next allocation in linked list
    struct allocation_header *prev; // Previous allocation in linked list
} allocation_header_t;

#define ALLOCATION_MAGIC 0xDEADBEEF
#define MIN_ALLOCATION_SIZE 16
#define HEADER_SIZE sizeof(allocation_header_t)

// Global allocation tracking
static allocation_header_t *allocation_list = NULL;
static size_t total_allocated = 0;
static size_t allocation_count = 0;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *dest, int value, size_t count)
{
	uint8_t val = (uint8_t)(value & 0xFF);
	uint8_t *dest2 = (uint8_t*)(dest);

	size_t i = 0;

	while(i < count)
	{
		dest2[i] = val;
		i++;
	}

	return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Helper function to align size to page boundaries
static size_t align_size(size_t size) {
    size_t page_size = 4096; // BITMAP_BLOCK_SIZE
    return (size + page_size - 1) & ~(page_size - 1);
}

// Calculate how many pages needed for a given size
static size_t pages_needed(size_t size) {
    return (size + HEADER_SIZE + 4095) / 4096; // Round up to pages
}

void *malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // Ensure minimum allocation size
    if (size < MIN_ALLOCATION_SIZE) {
        size = MIN_ALLOCATION_SIZE;
    }

    // Calculate total size needed including header
    size_t total_size = size + HEADER_SIZE;
    size_t num_pages = pages_needed(total_size);

    // Allocate physical pages
    void *pages = physical_alloc_pages(num_pages);
    if (!pages) {
        return NULL;
    }

    // Set up allocation header
    allocation_header_t *header = (allocation_header_t *)pages;
    header->size = size;
    header->magic = ALLOCATION_MAGIC;
    header->next = allocation_list;
    header->prev = NULL;

    // Update linked list
    if (allocation_list) {
        allocation_list->prev = header;
    }
    allocation_list = header;

    // Update statistics
    total_allocated += num_pages * 4096;
    allocation_count++;

    // Return pointer after header
    return (void *)((uint8_t *)pages + HEADER_SIZE);
}

void free(void *ptr) {
    if (!ptr) {
        return;
    }

    // Get header from user pointer
    allocation_header_t *header = (allocation_header_t *)((uint8_t *)ptr - HEADER_SIZE);

    // Validate magic number
    if (header->magic != ALLOCATION_MAGIC) {
        return; // Invalid pointer or corruption
    }

    // Calculate number of pages to free
    size_t total_size = header->size + HEADER_SIZE;
    size_t num_pages = pages_needed(total_size);

    // Remove from linked list
    if (header->prev) {
        header->prev->next = header->next;
    } else {
        allocation_list = header->next;
    }
    
    if (header->next) {
        header->next->prev = header->prev;
    }

    // Clear magic number to prevent double-free
    header->magic = 0;

    // Free the pages
    physical_free_pages(header, num_pages);

    // Update statistics
    if (total_allocated >= num_pages * 4096) {
        total_allocated -= num_pages * 4096;
    }
    if (allocation_count > 0) {
        allocation_count--;
    }
}

void *calloc(size_t nmemb, size_t size) {
    // Check for overflow
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return NULL;
    }

    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // Get current allocation info
    allocation_header_t *header = (allocation_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    
    if (header->magic != ALLOCATION_MAGIC) {
        return NULL; // Invalid pointer
    }

    size_t old_size = header->size;
    
    // If new size is smaller or same, just update size and return same pointer
    if (size <= old_size) {
        header->size = size;
        return ptr;
    }

    // Need to allocate new memory
    void *new_ptr = malloc(size);
    if (!new_ptr) {
        return NULL;
    }

    // Copy old data
    memcpy(new_ptr, ptr, old_size);
    
    // Free old memory
    free(ptr);
    
    return new_ptr;
}

// Kernel-specific allocation functions (aliases)
void *kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void *ptr) {
    free(ptr);
}

void *kmalloc_aligned(size_t size, size_t alignment) {
    // For simplicity, just use regular malloc since our pages are already aligned
    // In a more sophisticated implementation, we would handle custom alignments
    (void)alignment; // Suppress unused parameter warning
    return malloc(size);
}

// Memory allocation statistics
size_t malloc_get_total_allocated(void) {
    return total_allocated;
}

size_t malloc_get_free_memory(void) {
    return physical_get_free_memory();
}

void malloc_print_stats(size_t x, size_t y) {
    kprintf(x, y, "Malloc Stats:");
    kprintf(x, y += 15, "Total allocated: %d KB", total_allocated / 1024);
    kprintf(x, y += 15, "Active allocations: %d", allocation_count);
    kprintf(x, y += 15, "Free memory: %d KB", malloc_get_free_memory() / 1024);
}