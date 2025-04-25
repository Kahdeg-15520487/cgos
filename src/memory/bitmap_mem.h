#ifndef BITMAP_MEM_H
#define BITMAP_MEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Configuration
#define BITMAP_BLOCK_SIZE 4096 // 4KB per block, typical page size
#define BITMAP_MAX_BLOCKS 32768 // Maximum number of blocks to manage (128MB with 4KB blocks)

// Bitmap memory manager structure
typedef struct {
    uint8_t* bitmap;           // Pointer to bitmap storage
    uintptr_t memory_base;     // Base address of managed memory
    size_t total_blocks;       // Total number of blocks tracked
    size_t free_blocks;        // Number of free blocks
    size_t bitmap_size;        // Size of bitmap in bytes
} bitmap_memory_manager_t;

// Initialize the bitmap memory manager
bool bitmap_init(bitmap_memory_manager_t* manager, void* bitmap_storage, 
                 uintptr_t memory_base, size_t memory_size);

// Allocate a specified number of contiguous blocks
void* bitmap_alloc_blocks(bitmap_memory_manager_t* manager, size_t count);

// Allocate a single block
void* bitmap_alloc_block(bitmap_memory_manager_t* manager);

// Free previously allocated blocks
void bitmap_free_blocks(bitmap_memory_manager_t* manager, void* address, size_t count);

// Free a single block
void bitmap_free_block(bitmap_memory_manager_t* manager, void* address);

// Get the current free block count
size_t bitmap_get_free_blocks(bitmap_memory_manager_t* manager);

// Check if an address is managed by this bitmap
bool bitmap_contains_address(bitmap_memory_manager_t* manager, void* address);

// Convert between block index and memory address
size_t bitmap_address_to_block(bitmap_memory_manager_t* manager, void* address);
void* bitmap_block_to_address(bitmap_memory_manager_t* manager, size_t block);

// Add this to bitmap_mem.h if needed
bool bitmap_test_bit(uint8_t *bitmap, size_t bit);

#endif // BITMAP_MEM_H