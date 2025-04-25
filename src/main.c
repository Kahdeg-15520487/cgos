#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <stdarg.h>
#include <stdio.h>
#include "memory.h"
#include "pmm.h"
#include "graphic.h"

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST ,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// The following functions define a portable implementation of rand and srand.

static unsigned long int next = 1;  // NB: "unsigned long int" is assumed to be 32 bits wide

int rand(void)  // RAND_MAX assumed to be 32767
{
    next = next * 1103515245 + 12345;
    return (unsigned int) (next / 65536) % 32768;
}

void srand(unsigned int seed)
{
    next = seed;
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
    
    // Initialize random number generator
    srand(__TIME__[7] + __TIME__[6] * 10 + __TIME__[4] * 60 + __TIME__[3] * 600 + __TIME__[1] * 3600 + __TIME__[0] * 36000); // Seed with compile time value based on __TIME__

    // setup graphic
    setup_graphic(&framebuffer_request);
    // Get the framebuffer information
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    size_t width = framebuffer->width;
    size_t height = framebuffer->height;

    clear_screen(0x6495ed); // Clear the screen with cornflower blue color
    
    // Initialize physical memory manager
    if (physical_memory_init(memmap_request.response)) {
        kprintf(10, 320, "Physical memory manager initialized successfully");
        
        // Test physical memory allocation
        void *page1 = physical_alloc_page();
        void *page2 = physical_alloc_page();
        void *pages = physical_alloc_pages(4);
        
        kprintf(10, 335, "Allocated page 1 at: %p", page1);
        kprintf(10, 350, "Allocated page 2 at: %p", page2);
        kprintf(10, 365, "Allocated 4 contiguous pages at: %p", pages);
        
        // Free the pages
        physical_free_page(page1);
        physical_free_page(page2);
        physical_free_pages(pages, 4);
        
        kprintf(10, 380, "Freed all allocated pages");
        
        // Print memory statistics
        physical_print_stats();

        
    } else {
        kprintf(10, 320, "Failed to initialize physical memory manager");
    }

    // draw random color lines
    for (size_t i = 0; i < 100; i++) {
        uint32_t random_color = ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 8) | (rand() & 0xFF); // Generate a random color
        for (size_t j = 0; j < 10; j++) { // Increase the line width to 10 pixels
            volatile uint32_t *fb_ptr = framebuffer->address;
            fb_ptr[(i * (framebuffer->pitch / 4)) + (i + j)] = random_color;
        }
    }

    // Draw "Hello, World!" on the screen
    draw_string(10, 10, "Hello, World!", 0xff00000); // Red color
    draw_string(10, 25, "Hello, World!", 0x00ff00); // Green color
    draw_string(10, 40, "Hello, World!", 0x0000ff); // Blue color
    draw_string(10, 55, "Hello, World!", 0xffff00); // Yellow color

    // Example usage of kprintf
    kprintf(10, 70, " height: %d, width: %d.", height, width);
    kprintf(10, 90, " height: %d, width: %d.", height, width);

    // draw a box around the screen
    draw_rect(0, 0, width, height, 1, 0xf080FF, false);

    // draw_line(0,0,width-1,0,1, 0x0080FF);
    // draw_line(0,0,0,height-1,1, 0x0080FF);
    // draw_line(width-1,0,width-1,height-1,1, 0x0080FF);
    // draw_line(0,height-1,width-1,height-1,1, 0x0080FF);

    // draw_line(1,1,width-2,1,1, 0x0080FF);
    // draw_line(1,1,1,height-2,1, 0x0080FF);
    // draw_line(width-2,1,width-2,height-2,1, 0x0080FF);
    // draw_line(1,height-2,width-2,height-2,1, 0x0080FF);

    // draw_line(2,2,width-3,2,1, 0x0080FF);
    // draw_line(2,2,2,height-3,1, 0x0080FF);
    // draw_line(width-3,2,width-3,height-3,1, 0x0080FF);
    // draw_line(2,height-3,width-3,height-3,1, 0x0080FF);

    // Draw a rectangle in the center of the screen
    // draw_rect((width / 2) - 50, (height / 2) - 50, 100, 100,2, 0xFF00FF, false); // Purple filled rectangle
    // draw_rect((width / 2) - 50 - 2, (height / 2) - 50 - 2, 102, 102,2, 0xFFFF00, false); // Yellow unfilled rectangle
    // draw_rect((width / 2) - 50 - 4, (height / 2) - 50 - 4, 104, 104,2, 0x00FFFF, false); // Cyan filled rectangle


    // // Draw a circle in the center of the screen
    // draw_circle(width / 2, height / 2, 47,1, 0xFF0000, false); // Red filled circle
    // draw_circle(width / 2, height / 2, 48,1, 0x00FF00, false); // Green unfilled circle
    // draw_circle(width / 2, height / 2, 49,1, 0x0000FF, false); // Blue filled circle

    // Fetch the memory map entries.
    // uint64_t mmapentrycount = memmap_request.response->entry_count;
    // uint64_t entryindex = 0;
    // for (uint64_t i = 0; i < mmapentrycount; i++) {
    //     struct limine_memmap_entry *entry = memmap_request.response->entries[i];
    //     if (entry->type != LIMINE_MEMMAP_USABLE) {
    //         // continue; // Skip non-usable memory entries
    //     }
    //     entryindex++;
    //     int sizeKB = entry->length / 1024; // Convert length to KB
    //     int sizeMB = sizeKB / 1024; // Convert length to MB
    //     // Print the memory map entry information.
    //     switch (entry->type) {
    //     case LIMINE_MEMMAP_USABLE:
    //         kprintf(10, 110 + (i * 15), "Entry %d: Usable Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
    //     case LIMINE_MEMMAP_RESERVED:
    //         kprintf(10, 110 + (i * 15), "Entry %d: Reserved Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
    //     case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
    //         kprintf(10, 110 + (i * 15), "Entry %d: ACPI Reclaimable Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
    //     case LIMINE_MEMMAP_ACPI_NVS:
    //         kprintf(10, 110 + (i * 15), "Entry %d: ACPI NVS Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
    //     case LIMINE_MEMMAP_BAD_MEMORY:
    //         kprintf(10, 110 + (i * 15), "Entry %d: Bad Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
    //     case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
    //         kprintf(10, 110 + (i * 15), "Entry %d: Bootloader Reclaimable Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
    //     case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
    //         kprintf(10, 110 + (i * 15), "Entry %d: Kernel and Modules Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
    //     case LIMINE_MEMMAP_FRAMEBUFFER:
    //         kprintf(10, 110 + (i * 15), "Entry %d: Framebuffer Memory: Base: %lx, Length: %d KB, %d MB", i, entry->base, sizeKB, sizeMB);
    //         break;
        
    //     default:
    //         break;
    //     }
    // }

    // We're done, just hang...
    hcf();
}