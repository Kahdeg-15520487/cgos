#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

// Kernel memory allocation functions
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

// Kernel-specific allocation functions
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kmalloc_aligned(size_t size, size_t alignment);

// Memory allocation statistics
size_t malloc_get_total_allocated(void);
size_t malloc_get_free_memory(void);
void malloc_print_stats(size_t x, size_t y);

#endif // MEMORY_H