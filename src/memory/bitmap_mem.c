#include "bitmap_mem.h"
#include "memory.h"

// Bit manipulation macros
#define BITMAP_SET_BIT(bitmap, bit) ((bitmap)[(bit) / 8] |= (1 << ((bit) % 8)))
#define BITMAP_CLEAR_BIT(bitmap, bit) ((bitmap)[(bit) / 8] &= ~(1 << ((bit) % 8)))
#define BITMAP_TEST_BIT(bitmap, bit) ((bitmap)[(bit) / 8] & (1 << ((bit) % 8)))

bool bitmap_init(bitmap_memory_manager_t* manager, void* bitmap_storage, 
                 uintptr_t memory_base, size_t memory_size) {
    if (!manager || !bitmap_storage || memory_size == 0) {
        return false;
    }

    // Calculate the number of blocks that can be managed
    manager->total_blocks = memory_size / BITMAP_BLOCK_SIZE;
    if (manager->total_blocks > BITMAP_MAX_BLOCKS) {
        manager->total_blocks = BITMAP_MAX_BLOCKS;
    }

    // Calculate bitmap size in bytes (rounded up)
    manager->bitmap_size = (manager->total_blocks + 7) / 8;
    manager->bitmap = (uint8_t*)bitmap_storage;
    
    // Clear the bitmap (all memory free)
    memset(manager->bitmap, 0, manager->bitmap_size);
    
    manager->memory_base = memory_base;
    manager->free_blocks = manager->total_blocks;

    return true;
}

void* bitmap_alloc_block(bitmap_memory_manager_t* manager) {
    if (!manager || manager->free_blocks == 0) {
        return NULL;
    }

    // Find the first free block
    for (size_t i = 0; i < manager->total_blocks; i++) {
        if (!BITMAP_TEST_BIT(manager->bitmap, i)) {
            // Mark block as used
            BITMAP_SET_BIT(manager->bitmap, i);
            manager->free_blocks--;
            
            // Calculate and return the block address
            return bitmap_block_to_address(manager, i);
        }
    }

    return NULL; // No free blocks found
}

void* bitmap_alloc_blocks(bitmap_memory_manager_t* manager, size_t count) {
    if (!manager || count == 0 || count > manager->free_blocks) {
        return NULL;
    }

    if (count == 1) {
        return bitmap_alloc_block(manager);
    }

    // Find a sequence of 'count' free blocks
    size_t start_block = 0;
    size_t contiguous_count = 0;

    for (size_t i = 0; i < manager->total_blocks; i++) {
        if (!BITMAP_TEST_BIT(manager->bitmap, i)) {
            // If this is the start of a new sequence
            if (contiguous_count == 0) {
                start_block = i;
            }
            
            contiguous_count++;
            
            // Found enough contiguous blocks
            if (contiguous_count == count) {
                // Mark all blocks as used
                for (size_t j = 0; j < count; j++) {
                    BITMAP_SET_BIT(manager->bitmap, start_block + j);
                }
                
                manager->free_blocks -= count;
                return bitmap_block_to_address(manager, start_block);
            }
        } else {
            contiguous_count = 0; // Reset counter on used block
        }
    }

    return NULL; // Not enough contiguous free blocks
}

void bitmap_free_block(bitmap_memory_manager_t* manager, void* address) {
    if (!manager || !bitmap_contains_address(manager, address)) {
        return;
    }

    size_t block = bitmap_address_to_block(manager, address);
    
    // Only free if the block is currently marked as used
    if (BITMAP_TEST_BIT(manager->bitmap, block)) {
        BITMAP_CLEAR_BIT(manager->bitmap, block);
        manager->free_blocks++;
    }
}

void bitmap_free_blocks(bitmap_memory_manager_t* manager, void* address, size_t count) {
    if (!manager || !bitmap_contains_address(manager, address) || count == 0) {
        return;
    }

    size_t start_block = bitmap_address_to_block(manager, address);
    
    // Make sure we don't exceed the bounds of the memory region
    size_t max_blocks = count;
    if (start_block + count > manager->total_blocks) {
        max_blocks = manager->total_blocks - start_block;
    }
    
    // Free each block
    for (size_t i = 0; i < max_blocks; i++) {
        if (BITMAP_TEST_BIT(manager->bitmap, start_block + i)) {
            BITMAP_CLEAR_BIT(manager->bitmap, start_block + i);
            manager->free_blocks++;
        }
    }
}

size_t bitmap_get_free_blocks(bitmap_memory_manager_t* manager) {
    return manager ? manager->free_blocks : 0;
}

bool bitmap_contains_address(bitmap_memory_manager_t* manager, void* address) {
    if (!manager) {
        return false;
    }
    
    uintptr_t addr = (uintptr_t)address;
    uintptr_t end_addr = manager->memory_base + 
                         (manager->total_blocks * BITMAP_BLOCK_SIZE);
    
    return (addr >= manager->memory_base && addr < end_addr);
}

size_t bitmap_address_to_block(bitmap_memory_manager_t* manager, void* address) {
    uintptr_t addr = (uintptr_t)address;
    uintptr_t offset = addr - manager->memory_base;
    
    return offset / BITMAP_BLOCK_SIZE;
}

void* bitmap_block_to_address(bitmap_memory_manager_t* manager, size_t block) {
    return (void*)(manager->memory_base + (block * BITMAP_BLOCK_SIZE));
}