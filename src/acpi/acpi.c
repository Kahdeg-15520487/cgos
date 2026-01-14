/**
 * ACPI Implementation for CGOS
 * 
 * Note: Limine doesn't map legacy BIOS ROM (0xE0000-0xFFFFF) in HHDM,
 * so we use QEMU-specific shutdown/reboot ports instead of parsing ACPI tables.
 */

#include "acpi.h"
#include "../pci/pci.h"
#include "../debug/debug.h"

// Port I/O
extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
extern uint16_t inw(uint16_t port);
extern void outw(uint16_t port, uint16_t value);

// ACPI state
static bool acpi_available = false;

int acpi_init(void) {
    DEBUG_INFO("ACPI: Initializing (QEMU mode)...\n");
    
    // Since Limine doesn't map legacy BIOS ROM,
    // we'll just use QEMU-specific power management ports
    // which don't require ACPI table parsing
    
    acpi_available = true;
    DEBUG_INFO("ACPI: Initialized (using QEMU ports)\n");
    
    return 0;
}

bool acpi_is_available(void) {
    return acpi_available;
}

void acpi_shutdown(void) {
    DEBUG_INFO("ACPI: Initiating shutdown...\n");
    
    // QEMU PIIX4 PM shutdown port
    outw(0x604, 0x2000);
    
    // Alternative: bochs/older QEMU
    outw(0xB004, 0x2000);
    
    // Fallback: halt
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

void acpi_reboot(void) {
    DEBUG_INFO("ACPI: Initiating reboot...\n");
    
    // Try keyboard controller reset (8042)
    uint8_t val = 0x02;
    while (val & 0x02) {
        val = inb(0x64);
    }
    outb(0x64, 0xFE);  // Pulse reset line
    
    // If that didn't work, try QEMU reset
    outb(0x92, 0x01);  // Fast A20 gate reset
    
    // Fallback: triple fault
    __asm__ volatile(
        "lidt 0\n"
        "int3"
    );
    
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}
