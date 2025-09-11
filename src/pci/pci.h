#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// PCI Configuration Space Registers
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// PCI Header Type 0 offsets
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS_CODE      0x0B
#define PCI_CACHE_LINE_SIZE 0x0C
#define PCI_LATENCY_TIMER   0x0D
#define PCI_HEADER_TYPE     0x0E
#define PCI_BIST            0x0F
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_CARDBUS_CIS     0x28
#define PCI_SUBSYSTEM_VENDOR_ID 0x2C
#define PCI_SUBSYSTEM_ID    0x2E
#define PCI_EXPANSION_ROM   0x30
#define PCI_CAPABILITIES    0x34
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D

// PCI Command Register bits
#define PCI_COMMAND_IO          0x01
#define PCI_COMMAND_MEMORY      0x02
#define PCI_COMMAND_MASTER      0x04
#define PCI_COMMAND_SPECIAL     0x08
#define PCI_COMMAND_INVALIDATE  0x10
#define PCI_COMMAND_VGA_PALETTE 0x20
#define PCI_COMMAND_PARITY      0x40
#define PCI_COMMAND_WAIT        0x80
#define PCI_COMMAND_SERR        0x100
#define PCI_COMMAND_FAST_BACK   0x200
#define PCI_COMMAND_INTX_DISABLE 0x400

// PCI Class Codes
#define PCI_CLASS_NETWORK       0x02
#define PCI_SUBCLASS_ETHERNET   0x00

// E1000 Vendor/Device IDs
#define E1000_VENDOR_ID         0x8086
#define E1000_DEVICE_ID_82540EM 0x100E
#define E1000_DEVICE_ID_82545EM 0x100F
#define E1000_DEVICE_ID_82574L  0x10D3

#define MAX_PCI_DEVICES 256

typedef struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint32_t bar[6];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
} pci_device_t;

// PCI functions
void pci_init(void);
uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_config_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_config_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);
int pci_scan_devices(void);
pci_device_t *pci_get_device(int index);
int pci_get_device_count(void);
void pci_print_devices(int start_x, int start_y);

// I/O port functions (these should be implemented in your kernel)
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile ("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

#endif // PCI_H
