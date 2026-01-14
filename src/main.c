#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <stdarg.h>
#include <stdio.h>
#include "memory.h"
#include "pmm.h"
#include "memory/vmm.h"
#include "graphic.h"
#include "pci/pci.h"
#include "network/network.h"
#include "network/socket.h"
#include "network/dhcp.h"
#include "debug/debug.h"
#include "interrupt/interrupt.h"
#include "timer/timer.h"

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
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
    
    // Initialize debug console (QEMU debugcon) - simple test first
    debug_init();
    
    // Initialize physical memory manager
    if (physical_memory_init(memmap_request.response)) {
        kprintf(10, 110, "Physical memory manager initialized successfully");
        
        // Initialize HHDM (Higher Half Direct Map) from Limine
        if (hhdm_request.response == NULL) {
            kprintf(10, 125, "ERROR: HHDM not available from bootloader");
            DEBUG_ERROR("HHDM response is NULL - cannot continue\n");
            hcf();
        }
        vmm_set_hhdm_offset(hhdm_request.response->offset);
        kprintf(10, 125, "HHDM initialized successfully");
        
        // Initialize virtual memory manager
        kprintf(10, 140, "Initializing virtual memory manager...");
        DEBUG_INFO("Starting virtual memory manager initialization\n");
        if (vmm_init() == 0) {
            kprintf(10, 155, "Virtual memory manager initialized successfully");
            DEBUG_INFO("Virtual memory manager initialization completed\n");
        } else {
            kprintf(10, 155, "ERROR: Failed to initialize virtual memory manager");
            DEBUG_ERROR("Virtual memory manager initialization failed\n");
            hcf();
        }
        
        // Initialize interrupt system
        kprintf(10, 155, "Initializing interrupt system...");
        DEBUG_INFO("Starting interrupt system initialization\n");
        interrupt_init();
        kprintf(10, 170, "Interrupt system initialized successfully");
        DEBUG_INFO("Interrupt system initialization completed\n");
        
        // Timer system disabled - needs more debugging
        // TODO: Timer interrupts cause triple fault, need to investigate:
        // - IDT setup appears correct
        // - Assembly handler has proper register save/restore
        // - Issue may be with GDT selector or TSS configuration
        // timer_init();
        
        // Test physical memory allocation
        void *page1 = physical_alloc_page();
        void *page2 = physical_alloc_page();
        void *pages = physical_alloc_pages(4);
        
        kprintf(10, 185, "Allocated page 1 at: %p", page1);
        kprintf(10, 200, "Allocated page 2 at: %p", page2);
        kprintf(10, 215, "Allocated 4 contiguous pages at: %p", pages);
        
        // Print memory statistics
        physical_print_stats(10, 230);
        // Draw the memory bitmap visualization
        kprintf(10, 325, "Memory Bitmap Visualization:");
        draw_memory_bitmap(10, 340, 600, 150);
        
        // Free the pages
        physical_free_page(page1);
        physical_free_page(page2);
        physical_free_pages(pages, 4);
        
        kprintf(130, 230, "Freed all allocated pages");


    } else {
        kprintf(10, 350, "Failed to initialize physical memory manager");
    }

    // draw random color lines
    for (size_t i = 0; i < 100; i++) {
        uint32_t random_color = ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 8) | (rand() & 0xFF); // Generate a random color
        for (size_t j = 0; j < 10; j++) { // Increase the line width to 10 pixels
            volatile uint32_t *fb_ptr = framebuffer->address;
            fb_ptr[(i * (framebuffer->pitch / 4)) + (i + j + 300)] = random_color;
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

    // Initialize and demo the network stack
    kprintf(10, 525, "=== Network Stack Demo ===");
    
    // Initialize PCI subsystem
    kprintf(10, 540, "Initializing PCI subsystem...");
    DEBUG_INFO("Starting PCI subsystem initialization\n");
    pci_init();
    DEBUG_INFO("PCI subsystem initialization completed\n");
    kprintf(10, 555, "PCI bus scan completed");
    
    // Print discovered PCI devices to debug console only
    pci_print_devices(0, 0); // Parameters are ignored now since we use debug console
    
    // Initialize the network stack
    kprintf(10, 570, "Initializing network stack...");
    DEBUG_INFO("Starting network stack initialization\n");
    
    if (network_init() == 0) {
        kprintf(10, 585, "Network stack initialized successfully");
        DEBUG_INFO("Network stack initialization completed successfully\n");
        kprintf(10, 600, "Checking network interfaces...");
        
        // Real Network Demo - Attempt actual DHCP
        network_interface_t *eth_iface = network_get_interface(1); // eth0 interface
        if (eth_iface) {
            kprintf(10, 585, "Found ethernet interface: %s", eth_iface->name);
            kprintf(10, 600, "MAC Address: %x:%x:%x:%x:%x:%x", 
                   eth_iface->mac_address[0], eth_iface->mac_address[1], 
                   eth_iface->mac_address[2], eth_iface->mac_address[3],
                   eth_iface->mac_address[4], eth_iface->mac_address[5]);
            
            // Check if this is a real E1000 interface or stub
            if (eth_iface->mac_address[0] == 0x52 && eth_iface->mac_address[1] == 0x54) {
                kprintf(10, 615, "E1000 hardware driver active");
                kprintf(10, 630, "Real network hardware detected");
            } else {
                kprintf(10, 615, "Using stub ethernet interface");
                kprintf(10, 600, "E1000 MMIO now enabled (virtual memory active)");
            }
            
            // Initialize DHCP client for real network communication
            kprintf(10, 615, "Initializing DHCP client...");
            if (dhcp_client_init(eth_iface) == 0) {
                dhcp_client_t *dhcp_client = dhcp_get_client(eth_iface);
                
                if (dhcp_client) {
                    kprintf(10, 630, "DHCP client initialized successfully");
                    kprintf(10, 645, "Sending DHCP DISCOVER to network...");
                    
                    // Start real DHCP process (would send actual packets)
                    if (dhcp_client_start(dhcp_client) == 0) {
                        kprintf(10, 660, "DHCP DISCOVER packet prepared");
                        kprintf(10, 675, "Network demo completed successfully");
                        
                        // Show current interface status
                        if (eth_iface->ip_address == 0) {
                            kprintf(10, 690, "Interface status: No IP address assigned");
                            kprintf(10, 705, "IP: 0.0.0.0 (waiting for DHCP response)");
                        } else {
                            kprintf(10, 690, "Interface status: IP address configured");
                            kprintf(10, 705, "IP: %d.%d.%d.%d", 
                                   (eth_iface->ip_address >> 24) & 0xFF,
                                   (eth_iface->ip_address >> 16) & 0xFF,
                                   (eth_iface->ip_address >> 8) & 0xFF,
                                   eth_iface->ip_address & 0xFF);
                        }
                        
                        kprintf(10, 720, "Real network packets transmitted to QEMU");
                    } else {
                        kprintf(10, 645, "Failed to start DHCP client");
                    }
                } else {
                    kprintf(10, 645, "Failed to get DHCP client instance");
                }
            } else {
                kprintf(10, 630, "Failed to initialize DHCP client");
            }
        } else {
            kprintf(10, 555, "No ethernet interface found");
            
            // Show loopback interface instead
            network_interface_t *lo_iface = network_get_interface(0); // loopback
            if (lo_iface) {
                kprintf(10, 570, "Using loopback interface: %s", lo_iface->name);
                kprintf(10, 585, "IP: %d.%d.%d.%d", 
                       (lo_iface->ip_address >> 24) & 0xFF,
                       (lo_iface->ip_address >> 16) & 0xFF,
                       (lo_iface->ip_address >> 8) & 0xFF,
                       lo_iface->ip_address & 0xFF);
                kprintf(10, 600, "Status: Active (software-only)");
            } else {
                kprintf(10, 570, "No network interfaces available");
            }
        }
        
        kprintf(10, 735, "Network initialization completed");
    } else {
        kprintf(10, 525, "ERROR: Network stack initialization failed!");
        kprintf(10, 540, "System continuing without network support");
    }
    
    kprintf(10, 750, "System boot completed - entering main loop");
    DEBUG_INFO("Entering main loop for packet processing\n");
    
    // Get ethernet interface for polling
    network_interface_t *poll_iface = network_get_interface(1);
    dhcp_client_t *poll_dhcp = poll_iface ? dhcp_get_client(poll_iface) : NULL;
    
    if (poll_iface) {
        DEBUG_INFO("Polling interface: %s active=%d\n", poll_iface->name, poll_iface->active);
    } else {
        DEBUG_ERROR("No ethernet interface found for polling!\n");
    }
    
    uint32_t loop_count = 0;
    uint32_t last_ip = 0;
    uint32_t dhcp_retry_counter = 0;
    
    // Main loop - poll for packets
    while (1) {
        // Process incoming packets on all interfaces
        network_process_packets();
        
        // Update DHCP client state
        if (poll_dhcp) {
            dhcp_client_update(poll_dhcp);
            
            // Check if IP address changed (DHCP completed)
            if (poll_iface->ip_address != 0 && poll_iface->ip_address != last_ip) {
                last_ip = poll_iface->ip_address;
                DEBUG_INFO("DHCP acquired IP: %d.%d.%d.%d\n",
                           (poll_iface->ip_address >> 24) & 0xFF,
                           (poll_iface->ip_address >> 16) & 0xFF,
                           (poll_iface->ip_address >> 8) & 0xFF,
                           poll_iface->ip_address & 0xFF);
                
                kprintf(10, 765, "IP acquired: %d.%d.%d.%d",
                       (poll_iface->ip_address >> 24) & 0xFF,
                       (poll_iface->ip_address >> 16) & 0xFF,
                       (poll_iface->ip_address >> 8) & 0xFF,
                       poll_iface->ip_address & 0xFF);
            }
            
            // Resend DHCP DISCOVER periodically if we don't have an IP
            dhcp_retry_counter++;
            if (poll_iface->ip_address == 0 && dhcp_retry_counter % 100000 == 0) {
                DEBUG_INFO("Resending DHCP DISCOVER (retry %u)...\n", dhcp_retry_counter / 100000);
                dhcp_client_discover(poll_dhcp);
            }
        }
        
        // Periodically log that we're still running
        loop_count++;
        if (loop_count % 1000000 == 0) {
            DEBUG_INFO("Main loop iteration %u (IP: %d.%d.%d.%d)\n", 
                       loop_count / 1000000,
                       poll_iface ? (poll_iface->ip_address >> 24) & 0xFF : 0,
                       poll_iface ? (poll_iface->ip_address >> 16) & 0xFF : 0,
                       poll_iface ? (poll_iface->ip_address >> 8) & 0xFF : 0,
                       poll_iface ? poll_iface->ip_address & 0xFF : 0);
        }
        
        // Small delay to avoid spinning too fast
        for (volatile int i = 0; i < 1000; i++);
    }
}